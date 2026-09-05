#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    struct stat st2;

    // 打开当前路径
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取当前路径对应文件的信息
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // find主要需要遍历目录
    if(st.type == T_DIR){

        // 防止路径太长导致buf溢出
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
            printf("find: path too long\n");
            close(fd);
            return;
        }

        // 先把当前路径复制进buf
        strcpy(buf, path);

        // p指向当前路径末尾
        p = buf + strlen(buf);

        // 在当前路径后加入 '/'
        *p++ = '/';

        // 一项一项读取目录
        while(read(fd, &de, sizeof(de)) == sizeof(de)){

            // 无效目录项，跳过
            if(de.inum == 0)
                continue;

            // 把当前目录项的名字接到路径后面
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            // 不允许递归进入 "." 和 ".."
            if(strcmp(p, ".") == 0 ||
               strcmp(p, "..") == 0)
                continue;

            // 如果文件名就是我们要找的名字
            if(strcmp(p, target) == 0){
                printf("%s\n", buf);
            }

            // 判断当前目录项是不是目录
            if(stat(buf, &st2) < 0){
                fprintf(2, "find: cannot stat %s\n", buf);
                continue;
            }

            // 如果是目录，就递归进入
            if(st2.type == T_DIR){
                find(buf, target);
            }
        }
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    if(argc != 3){
        fprintf(2, "usage: find path filename\n");
        exit(1);
    }

    find(argv[1], argv[2]);

    exit(0);
}