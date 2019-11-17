#ifndef __M_TCPSOCKET_H__
#define __M_TCPSOCKET_H__ 

#include <iostream>
#include <string>
#include <error.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

#define LISSIZE 10
#define BUFSIZE 2048

class Tcpsocket 
{
  public:
    Tcpsocket() :_sockfd(-1) {}

    bool TcpsockInit(const uint16_t port_tmp)
    {
      int _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if(_sockfd < 0)
      {
        cout << "socket error" << endl;
        return false;
      }

      string ip = "0.0.0.0";                                // 本机任意一个地址
      uint16_t port= htons(port_tmp);

      int opt = 1;                                          // 地址重复使用
      setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
      struct sockaddr_in sockaddr;
      sockaddr.sin_family = AF_INET;
      sockaddr.sin_addr.s_addr = inet_addr(ip.c_str());
      sockaddr.sin_port = port;
      socklen_t len = sizeof(sockaddr);

      int ret = bind(_sockfd, (struct sockaddr*)&sockaddr, len);
      if(ret < 0)
      {
        cout << "bind error" << endl;
        return false;
      }

      ret = listen(_sockfd, LISSIZE);
      if(ret < 0)
      {
        cout << "listen error" << endl;
        return false;
      }

      return true;
    }

    bool Accept(Tcpsocket& sock)
    {
      struct sockaddr_in sockaddr;                           // 保存客户端地址
      socklen_t len = sizeof(sockaddr);

      int newfd = accept(_sockfd, (struct sockaddr*)&sockaddr, &len);
      if(newfd < 0)
      {
        cout << "accept error" << endl;
        return false;
      }

      sock.Setfd(newfd);
      return true;
    }

    bool Recv(string& date, int len)
    {                                    // 当事件触发后，采用非阻塞循环读取的方式一次性读取数据。
      SetNonblock();
      char temp[BUFSIZE] = {0};
      int rlen = 0;
      while(rlen < len)
      {
        int ret = recv(_sockfd, temp + rlen, len - rlen, 0);
        if(ret < 0)
        {
          if(errno == EAGAIN)
          {
            usleep(1000);
            continue;
          }
          else 
          {
            cout << "recv error" << endl;
            return false;
          }
        }
        else if(ret == 0)
        {
          cout << "Peer closed" << endl;
          return false;
        }

        rlen += ret;
      }

      date.assign(temp, rlen);
      return true;
    }

    bool RecvPeek(string& date)
    {
      SetNonblock();
      char temp[BUFSIZE] = {0};
      int ret = recv(_sockfd, temp, BUFSIZE, MSG_PEEK);
      if(ret < 0)
      {
        if(errno == EAGAIN)
        {
          return true;
        }
        else 
        {
          cout << "recv error" << endl;
          return false;
        }
      }
      else if(ret == 0)
      {
        cout << "Peer closed" << endl;
        return false;
      }

      date.assign(temp, ret);
      return true;
    }

    bool Send(const string& date)
    {
      SetNonblock();
      int ret = send(_sockfd, date.c_str(), date.size(), 0);
      if(ret < 0)
      {
        if(errno == EAGAIN)
        {
          return true;
        }
        else 
        {
          cout << "send error" << endl;
          return false;
        }
      }

      return true;
    }

    int Getfd()
    {
      return _sockfd;
    }

    void Setfd(const int fd)
    {
      _sockfd = fd;
    }

  private:
    void SetNonblock()
    {
      int flags = fcntl(_sockfd, F_GETFD);
      fcntl(_sockfd, F_SETFD, flags | O_NONBLOCK);
    }

  private:
    int _sockfd;
};

#endif
