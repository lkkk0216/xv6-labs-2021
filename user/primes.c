#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int left_fd) 
{
    int prime;

    // 如果没有数字可读取了，说明已经到了末尾
    if (read(left_fd, &prime, sizeof(prime)) == 0) {
        close(left_fd);
        exit(0);
    }

    // 当前读到的第一个数字一定是质数
    printf("prime %d\n", prime);

    // 创建通往下一层筛选进程的管道
    int p[2];
    pipe(p);

    int pid = fork();

    if (pid == 0) {   //子进程
        // 子进程只需要从管道中读，只需要读取p[0]
        close(p[1]);        

        // 子进程不需要left_fd
        close(left_fd);

        // 递归进入下一层
        sieve(p[0]);

        exit(0);

    } else {    // 父进程
        // 父进程负责使用prime过滤数字并通过p[1]发给下一层
        // 父进程只需要往管道里面写入，不需要p[0]
        close(p[0]);

        int x;
        while (read(left_fd, &x, sizeof(x)) != 0) {
            // 筛掉prime的倍数
            if (x % prime != 0) {
                // 写入通往下一层的管道
                write(p[1], &x, sizeof(x));
            }
        }

        // 读完，左边没有数据了
        close(left_fd);

        // 关闭下一层管道的写入端，否则下一层无法读到EOF
        close(p[1]);

        // 等待下一层筛选进程结束
        wait(0);

        exit(0);
    }
}

int main(int argc, char *argv[]) 
{
    int p[2];

    // 创建第一根管道
    pipe(p);

    int pid = fork();

    if (pid == 0) {
        // 子进程：第一层筛选器
        close(p[1]);

        sieve(p[0]);

        exit(0);
    } else {
        // 父进程：生成2～35
        close(p[0]);

        for (int i = 2; i <= 35; i++) {
            // 将生成的待筛选数字送入第一个管道
            write(p[1], &i, sizeof(i));
        }

        // 全部写完，关闭p[1]，否则下一层无法读到EOF
        close(p[1]);

        // 等待整个流水线筛选完成
        wait(0);

        exit(0);
    }
}