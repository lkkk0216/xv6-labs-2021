#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    char buf[512];
    char *newargv[MAXARG];

    int bufpos = 0;
    int argpos = 0;
    int baseargc;
    int inarg = 0;
    char c;

    // xargs 后面至少要有一个要执行的命令
    if(argc < 2){
        fprintf(2, "usage: xargs command [args ...]\n");
        exit(1);
    }

    // 复制 xargs 后面的固定参数
    for(int i = 1; i < argc; i++){
        newargv[argpos++] = argv[i];
    }

    baseargc = argpos;

    // 一个字符一个字符地读取标准输入
    while(read(0, &c, 1) == 1){

        // 遇到换行：当前这一行结束
        if(c == '\n'){

            // 如果当前有一个尚未结束的参数
            if(inarg){
                buf[bufpos++] = '\0';
                inarg = 0;
            }

            // argv 最后必须以 0 结束
            newargv[argpos] = 0;

            // 创建子进程执行命令
            int pid = fork();

            if(pid < 0){
                fprintf(2, "xargs: fork failed\n");
                exit(1);
            }

            if(pid == 0){
                exec(newargv[0], newargv);

                // exec成功不会执行到这里
                fprintf(2, "xargs: exec failed\n");
                exit(1);
            }

            // 父进程等待子进程结束（子进程不会进行到这里）
            wait(0);

            // 准备读取下一行
            bufpos = 0;
            argpos = baseargc;
            inarg = 0;
        }

        // 遇到空格：当前参数结束
        else if(c == ' '){

            if(inarg){
                buf[bufpos++] = '\0';
                inarg = 0;
            }
        }

        // 普通字符
        else{

            // 新参数开始
            if(!inarg){

                if(argpos >= MAXARG - 1){
                    fprintf(2, "xargs: too many arguments\n");
                    exit(1);
                }

                newargv[argpos++] = &buf[bufpos];
                inarg = 1;
            }

            if(bufpos >= sizeof(buf) - 1){
                fprintf(2, "xargs: input too long\n");
                exit(1);
            }

            buf[bufpos++] = c;
        }
    }

    exit(0);
}