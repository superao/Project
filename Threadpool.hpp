#ifndef __M_THREADPOOL_H__
#define __M_THREADPOOL_H__ 
#include <iostream>
#include <pthread.h>
#include <queue>
#include <thread>
using namespace std;

#define MAXNODE 20
#define MAXTHREAD 20
typedef void*(*HandleFun)(int);

class Task 
{
  public:
    Task() {}

    Task(int date, HandleFun handlefun)
    {
      _date = date;
      _handlefun = handlefun;
    }

    void TaskRun()
    {
      _handlefun(_date);
    }

  private:
    int _date;
    HandleFun _handlefun;
};

class ThreadPool 
{
  public:

    ThreadPool(int nodesize = MAXNODE, int threadsize = MAXTHREAD) :_max_node_size(nodesize), _max_thread_size(threadsize) 
    {
      pthread_mutex_init(&_mutex, NULL);
      pthread_cond_init(&_con_cond, NULL);
      pthread_cond_init(&_pro_cond, NULL);
    }

    ~ThreadPool()
    {
      pthread_mutex_destroy(&_mutex);
      pthread_cond_destroy(&_pro_cond);
      pthread_cond_destroy(&_con_cond);
    }

    void ThreadInit()
    {
      for(int i = 0; i < _max_thread_size; ++i)
      {
        thread th(&ThreadPool::Th_Start, this);
        th.detach();
      }
    }

    bool Queue_Push(const Task& tt)
    {
      pthread_mutex_lock(&_mutex);
      while((int)_queue.size() >= _max_node_size)
      {
        pthread_cond_wait(&_pro_cond, &_mutex);
      }

      _queue.push(tt);
      pthread_mutex_unlock(&_mutex);
      pthread_cond_signal(&_con_cond);

      return true;
    }

    bool Queue_Pop(Task& tt)
    {
      pthread_mutex_lock(&_mutex);
      if(_queue.empty())
      {
        pthread_mutex_unlock(&_mutex);
      }
    
      tt = _queue.front();
      _queue.pop();

      pthread_mutex_unlock(&_mutex);
      return true;
    }

  private:
    void Th_Start()
    {
      pthread_mutex_lock(&_mutex);
      while(_queue.empty())
      {
        pthread_cond_wait(&_con_cond, &_mutex);
      }

      Task task;
      Queue_Pop(task);

      pthread_mutex_unlock(&_mutex);
      pthread_cond_signal(&_pro_cond);

      task.TaskRun() ;
    }

  private:
    queue<Task> _queue;
    int _max_node_size;

    pthread_mutex_t _mutex;
    pthread_cond_t _pro_cond;
    pthread_cond_t _con_cond;

    int _max_thread_size;
};

#endif 
