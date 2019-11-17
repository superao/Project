#ifndef __M_EPOLL_H__
#define __M_EPOLL_H__ 
#include <iostream>
#include <sys/epoll.h>
#include <vector>
#include "Tcpsocket.hpp"
using namespace std;

#define NODESIZE 2048         // 最大监控事件数量
#define WAITTIME 3000         // 阻塞等待时间
#define EVENTSIZE 20          // 事件数组大小

class Epoll 
{
  public:
    bool EpollInit()     // 初始化 Epoll
    {
      int _epfd = epoll_create(NODESIZE);
      if(_epfd < 0)
      {
        cout << "create error" << endl;
        return false;
      }

      return true;
    }
    
    bool EpollAdd(const int fd)      // 添加事件
    {
      struct epoll_event event;
      event.events = EPOLLIN;
      event.data.fd = fd;
      int ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &event);
      if(ret < 0)
      {
        cout << "ctl error" << endl;
        return false;
      }

      return true;
    }
    
    bool EpollDel(const int fd)      // 删除事件
    {
      struct epoll_event event;
      event.events = EPOLLIN;
      event.data.fd = fd;
      int ret = epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, &event);
      if(ret < 0)
      {
        cout << "ctl error" << endl;
        return false;
      }

      return true;
    }

    bool EpollWait(vector<Tcpsocket>& socklist)     // 监控事件
    {
      struct epoll_event event[EVENTSIZE];
      int ret = epoll_wait(_epfd, event, EVENTSIZE, WAITTIME);
      if(ret < 0)
      {
        cout << "epollwait error" << endl;
        return false;
      }
      else if(ret == 0)
      {
        cout << "Waiting timeout" << endl;
        return false;
      }

      for(int i = 0; i < 10; ++i)
      {
        Tcpsocket sock;
        int fd = event[i].data.fd;
        sock.Setfd(fd);
        socklist.push_back(sock);
      }

      return true;
    }

  private:
    int _epfd;
};

#endif
