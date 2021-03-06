#ifndef __M_TCPSOCKET_H__
#define __M_TCPSOCKET_H__ 

#include <iostream>
#include <string>
#include <sstream>
#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

#define LISSIZE 10          // Socket 最大监听数
#define BUFSIZE 10240       // 探测接受中设置的接受缓冲区大小

// 封装通信Socket
class Tcpsocket 
{
    public:
        typedef void (*func)(int avg);
        Tcpsocket() :_sockfd(-1) {}

        bool TcpsockInit(const uint16_t port_tmp)
        {
            _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if(_sockfd < 0)
            {
                cout << "Tcpsocket.hpp/TcpsockInit(): Socket Error" << endl;
                return false;
            }

            string ip = "0.0.0.0";                                // 本机任意一个地址
            uint16_t port= htons(port_tmp);                       // 用户指定端口

            // 地址重复使用
            int opt = 1;                                          
            setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
            struct sockaddr_in sockaddr;
            sockaddr.sin_family = AF_INET;
            sockaddr.sin_addr.s_addr = inet_addr(ip.c_str());
            sockaddr.sin_port = port;
            socklen_t len = sizeof(sockaddr);

            int ret = bind(_sockfd, (struct sockaddr*)&sockaddr, len);
            if(ret < 0)
            {
                cout << "Tcpsocket.hpp/TcpsockInit(): bind error" << endl;
                return false;
            }

            ret = listen(_sockfd, LISSIZE);
            if(ret < 0)
            {
                cout << "Tcpsocket.hpp/TcpsockInit(): listen error" << endl;
                return false;
            }

            return true;
        }

        // 获取客户端Socket
        bool Accept(Tcpsocket& sock)
        {
            struct sockaddr_in sockaddr;                           // 保存客户端地址
            socklen_t len = sizeof(sockaddr);

            // 获取客户端Socket描述符
            int newfd = accept(_sockfd, (struct sockaddr*)&sockaddr, &len);
            if(newfd < 0)
            {
                cout << "Tcpsocket.hpp/Accept(): accept error" << endl;
                return false;
            }

            sock.Setfd(newfd);
            return true;
        }

        // 接受数据(非阻塞 + 轮询)
        bool Recv(string& data, uint64_t len) 
        {
            SetNonblock();
            data.resize(len);
            uint64_t rlen = 0;
            while(rlen < len)
            {
                int ret = recv(_sockfd, &data[0] + rlen, len - rlen, 0);
                if(ret < 0)
                {
                    if(errno == EAGAIN)
                        continue;
                    else 
                    {
                        cout << "Tcpsocket.hpp/Recv(): recv error!" << endl;
                        perror("Recv error!");
                        return false;
                    }
                }
                else if(ret == 0)
                {
                    cout << "Tcpsocket.hpp/Recv(): Peer closed" << endl;
                    close(_sockfd);
                    return false;
                }

                rlen += ret;
            }

            return true;
        }

        // 探测接受数据
        bool RecvPeek(string& date)
        {
            SetNonblock();
            char temp[BUFSIZE] = {0};
            int ret = recv(_sockfd, temp, BUFSIZE, MSG_PEEK);
            if(ret < 0)
            {
                if(errno == EAGAIN)
                    return true;
                else 
                {
                    cout << "Tcpsocket.hpp/RecvPeek(): recv error" << endl;
                    perror("Recv error!");
                    return false;
                }
            }
            else if(ret == 0)
            {
                cout << "Tcpsocket.hpp/RecvPeek(): Peer closed" << endl;
                close(_sockfd);
                return false;
            }

            date.assign(temp, ret);
            return true;
        }

        // 发送数据
        // 短小数据采用阻塞发送
        bool Send(const string& date)
        {
            // 短小数据采用阻塞发送
            Setblock();
            int ret = send(_sockfd, date.c_str(), date.size(), 0);
            if(ret < 0)
            {
                {
                    cout << "Tcpsocket.hpp/Send(): send error!" << endl;
                    perror("Send error!");
                    return false;
                }
            }

            return true;
        }

        // 发送数据
        // 大量数据采用非阻塞循环发送
        bool Send(const string& date, uint64_t datelen)
        {
            // 大量数据采用非阻塞循环发送
            SetNonblock();
            uint64_t curlen = 0;
            while(curlen < datelen)
            {
                int ret = send(_sockfd, &date[0] + curlen, datelen - curlen, 0);
                if(ret < 0)
                {
                    if(errno == EAGAIN)
                        continue;
                    else 
                    {
                        cout << "Tcpsocket.hpp/Send(): send error!" << endl;
                        perror("Send error!");
                        return false;
                    }
                }

                curlen += ret;
            }

            return true;
        }

        // 获取Socket文件描述符
        int Getfd()
        {
            return _sockfd;
        }

        // 设置Socket文件描述符
        void Setfd(const int fd)
        {
            _sockfd = fd;
        }

        // 关闭文件描述符
        void Close()
        {
            close(_sockfd);
        }
        
    private:
        // 设置文件非阻塞状态
        void SetNonblock()
        {
            int flags = fcntl(_sockfd, F_GETFL);
            fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);
        }

        // 设置文件阻塞状态
        void Setblock()
        {
            int flags = fcntl(_sockfd, F_GETFL);
            fcntl(_sockfd, F_SETFL, flags & ~O_NONBLOCK);
        }

    private:
        int _sockfd;        // 记录socket文件描述符
};

#endif
