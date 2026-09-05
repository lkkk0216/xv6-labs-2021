// Buffer cache.
//
// 用哈希桶分开保护不同的磁盘块。缓存未命中时再用evict锁
// 挑选空闲buf，保证同一个磁盘块在缓存中只有一份。

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct bucket {
  struct spinlock lock;
  struct buf *head;
};

struct {
  struct spinlock evict;
  struct bucket bucket[NBUCKET];
  struct buf buf[NBUF];
} bcache;

static uint
bhash(uint blockno)
{
  return blockno % NBUCKET;
}

void
binit(void)
{
  initlock(&bcache.evict, "bcache.evict");
  for(int i = 0; i < NBUCKET; i++)
    initlock(&bcache.bucket[i].lock, "bcache.bucket");

  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    int h = i % NBUCKET;
    b->next = bcache.bucket[h].head;
    bcache.bucket[h].head = b;
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  uint h = bhash(blockno);
  struct bucket *bucket = &bcache.bucket[h];

  acquire(&bucket->lock);
  for(struct buf *b = bucket->head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  acquire(&bcache.evict);

  // 拿到evict锁以后要再检查一次，防止刚才已有CPU放入同一个块
  acquire(&bucket->lock);
  for(struct buf *b = bucket->head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      release(&bcache.evict);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  struct buf *victim = 0;
  for(int i = 0; i < NBUCKET && victim == 0; i++){
    struct bucket *from = &bcache.bucket[i];
    acquire(&from->lock);
    struct buf **p = &from->head;
    while(*p){
      if((*p)->refcnt == 0){
        victim = *p;
        *p = victim->next;
        break;
      }
      p = &(*p)->next;
    }
    release(&from->lock);
  }

  if(victim == 0){
    release(&bcache.evict);
    panic("bget: no buffers");
  }

  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;

  acquire(&bucket->lock);
  victim->next = bucket->head;
  bucket->head = victim;
  release(&bucket->lock);
  release(&bcache.evict);

  acquiresleep(&victim->lock);
  return victim;
}

struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b = bget(dev, blockno);
  if(!b->valid){
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  struct bucket *bucket = &bcache.bucket[bhash(b->blockno)];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}

void
bpin(struct buf *b)
{
  struct bucket *bucket = &bcache.bucket[bhash(b->blockno)];
  acquire(&bucket->lock);
  b->refcnt++;
  release(&bucket->lock);
}

void
bunpin(struct buf *b)
{
  struct bucket *bucket = &bcache.bucket[bhash(b->blockno)];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}
