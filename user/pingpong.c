#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int p2c[2];   // 父 -> 子
  int c2p[2];   // 子 -> 父
  char buf[1];

  pipe(p2c);
  pipe(c2p);

  int pid = fork();

  if(pid < 0){
    fprintf(2, "fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // 子进程：先收 ping，再回 pong
    read(p2c[0], buf, 1);
    printf("%d: received ping\n", getpid());
    write(c2p[1], buf, 1);
    exit(0);
  } else {
    // 父进程：先发 ping，再等 pong
    write(p2c[1], "x", 1);
    read(c2p[0], buf, 1);
    printf("%d: received pong\n", getpid());
    exit(0);
  }
}
