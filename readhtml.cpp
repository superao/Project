// 测试读取html文件到标准输出
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
using namespace std;

#define BUFSIZE 10240

void SetNonBlock(int fd)
{
    int ret = fcntl(fd, F_GETFL);
    ret |= O_NONBLOCK; 
    fcntl(fd, F_SETFL, ret);
}

int main()
{
    // 打开html文件
    int fd = open("./HTML/404page.html", O_RDWR);
    if(fd < 0)
    {
        cout << "Open error!" << endl;
    }

    // 设置文件非阻塞模式
    SetNonBlock(fd);

    // 获取文件大小
    int filelength = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // 循环读取文件内容，输出到标准输出
    int curread = 0;
    char buf[BUFSIZE];
    while(curread < filelength)
    {
        memset(buf, 0, BUFSIZE); 
        int ret = read(fd, buf, 8192);
        if(ret < 0)
        {
            cout << "read error!" << endl;
            close(fd);
            return -1; 
        }

        curread += ret;

        cout << buf;
    }

    cout << endl << "读取文件结束！" << endl;

    close(fd);
    return 0;
}
