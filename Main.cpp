#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include "Tcpsocket.hpp"
#include "Epoll.hpp"
#include "Threadpool.hpp"
#include "Http.hpp"
#include "Service.hpp"

using namespace std;

int main()
{
    // 创建子进程，父进程退出
    pid_t pid = fork();
    if(pid < 0)
    {
        cout << "Main(): fork error!" << endl;
        return -1;
    }
    else if(pid != 0)
    {
        // 父进程直接退出
        return 0;
    }

    // 子进程创建新的会话
    setsid();

    // 改变当前工作路径
    int ret = chdir("/home/superao/testgit/GitHub/Project");
    if(ret < 0)
    {
        cout << "Main(): chdir error" << endl;
        return -1;
    }

    // 设置文件掩码
    umask(0002);

    // 关闭继承下来的文件描述符
    close(0);
    close(1);
    close(2);

    // 核心任务
    while(1)
    {
        Service srv;
        int ret = srv.ServiceStart(8030);
        if(ret == false)
        {
            cout << "main(): ServiceStart error!" << endl;
            return -1;
        }
    }

    return 0;
}
