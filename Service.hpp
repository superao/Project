#ifndef __M_SERVICE_H__
#define __M_SERVICE_H__ 
#include <iostream>
#include <string>
#include <ctime>
#include <fstream>
#include <boost/filesystem.hpp>
#include <signal.h>
#include "Tcpsocket.hpp"
#include "Http.hpp"
#include "Threadpool.hpp"
#include "Epoll.hpp"
using namespace std;

#define SEVRVICEROOT "./DataResource"      // 服务器根目录
#define CGI "./upload"                     // 外部处理程序

#define INDEXHTML "./HTML/index.html"      // 服务器首页文件
#define LISTHTML "./HTML/list.html"        // 服务器列表文件
#define LISTTAIL "./HTML/list_tail.html"   // 服务器列表文件末尾
#define NOTFINDHTML "./HTML/404page.html"  // 服务器404页面

int clisockfd = 0;                         // 标识客户端套接字，当触发SIGPIPE时关闭当前套接字

void sighandle(const int date);            // SIGPIPE信号处理方式
bool ReadFile(stringstream& ss, const char* filename);

class Service 
{
    public:
        // 初始化成员变量
        bool ServiceStart(uint16_t port)
        {
            // 初始化套接字
            int ret = _lissock.TcpsockInit(port);
            if(ret == false)
            {
                cout << "Service.hpp/ServiceStart(): Tcpsocket error!" << endl;
                return false;
            }

            // 初始化 epoll
            ret = _ep.EpollInit();
            if(ret == false)
            {
                cout << "Service.hpp/ServiceStart(): EpollInit error!" << endl;
                return false;
            }

            // 初始化线程池
            _thpool.ThreadInit();

            // 获取监听套接字描述符
            int fd = _lissock.Getfd();
            _ep.EpollAdd(fd);
            while(1)
            {
                // epoll 监控
                vector<Tcpsocket> listsock;
                ret = _ep.EpollWait(listsock);
                if(ret == false)
                {
                    continue;
                }

                // 分类处理就绪事件
                for(auto& sock : listsock)
                {
                    if(sock.Getfd() == _lissock.Getfd())
                    {
                        // 监听套接字
                        Tcpsocket clisock;
                        ret = sock.Accept(clisock);
                        if(ret == false)
                            continue;

                        _ep.EpollAdd(clisock.Getfd());
                        
                        // 自定义客户端socket对SIGPIPE信号的处理
                        clisockfd = clisock.Getfd();
                        signal(SIGPIPE, sighandle);
                    }
                    else 
                    {
                        // 客户端套接字
                        Task tt;
                        tt.TaskAdd(sock.Getfd(), ThreadPoolHandle);
                        _thpool.Queue_Push(tt);
                        _ep.EpollDel(sock.Getfd());
                    }
                }
            }

            return true;
        }

        // 定义线程处理函数      (类中的静态函数如何调用？静态变量？)
        static bool ThreadPoolHandle(int date)
        {
            Tcpsocket clisock;
            clisock.Setfd(date);

            HttpRequest req;
            HttpResponse res;

            // 接受头部
            string headers;
            cout << "正在接受头部信息！" << endl;
            int ret = req.RecvHeader(clisock, headers);
            if(ret == false)
            {
                cout << "Service.hpp/ThreadPoolHandle(): RecvHeader error" << endl;
                return false;
            }

            // 解析头部
            int status = req.RequestParse(headers);
            if(status != 200 && status != 206)
            {
                // 响应错误页面
                res.SetStatus(status);
                res.ErrorResponse(clisock);
                clisock.Close();
                return true;
            }

            // 接受正文
            cout << "正在接受正文信息！" << endl;
            ret = req.RecvMessage(clisock);
            if(ret == false)
            {
                cout << "Service.hpp/ThreadPoolHandle(): RecvMessage error" << endl;
                return false;
            }
            cout << "正文信息接受完毕，进行数据处理！" << endl;

            // 调用请求与响应函数，根据不同的请求进行相应的响应
            ret = HttpHandle(req, res, clisock);
            if(ret == false)
            {
                cout << "Service.hpp/ThreadPoolHandle(): HttpHandle error" << endl;
                return false;
            }

            // 关闭套接字   (响应完毕后直接关闭了套接字->短链接   短链接长连接的区别？)
            clisock.Close();

            return true;
        }

        // 请求与响应函数(功能实现)
        /**********************************************************
         * 功能实现:
         * 一. 文件下载(GET) 和 文件列表(GET) 和 文件搜索(GET)
         * 三者区别:
         *    查询字符串为空时:
         *      当请求的文件路径是一个普通文件的时候，那么就是文件下载功能。
         *      当请求的文件路径是一个目录文件的时候，那么就是文件列表功能。
         *    查询字符串不为空时:
         *      当查询字符串不为空时，那么就是文件搜索功能。
         * 二. 文件上传(POST)
         *    当请求方法是POST时，那么就是文件上传功能。
         * *********************************************************/
        static bool HttpHandle(HttpRequest& req, HttpResponse& res, Tcpsocket& clisock)
        {
            // 用户请求路径
            string clipath = req._path.c_str();
            
            // 请求绝对路径
            string realpath = SEVRVICEROOT + clipath;

            // 请求方法
            string method = req._method.c_str();
            
            // 检测当前绝对路径的文件类型(文件0 / 目录1)
            bool fileflag = 0;
            if(boost::filesystem::exists(realpath) == true)
            {
                fileflag = boost::filesystem::is_directory(realpath);
            }

            // 文件列表功能
            if(method == "GET" && fileflag == true && req._reqstr.size() == 0)
            {
                cout << "进入文件列表功能" << endl;
                string body;
                string bodylength;
                int ret = ListShow(realpath, body, bodylength);
                if(ret == true)
                {
                    // 响应头部信息
                    res.SetStatus(200);
                    res.SetHeaders("Content-Length", bodylength);
                    res.SetHeaders("Content-Type", "text/html");
                    res.NormalResponseHeader(clisock);
                    
                    // 响应正文信息
                    res.SetBody(body);
                    res.NormalResponseBody(clisock);
                    return true;
                }
                else
                {
                    cout << "Service.hpp/HttpHandle(): List Show error" << endl;
                    return false;
                }
            }

            // 文件下载功能
            if(method == "GET" && fileflag == false && req._reqstr.size() == 0)
            {
                cout << "进入文件下载功能" << endl;
                // 正文长度
                string bodylength;

                // 检测当前请求是否为断点续传
                unordered_map<string, string>::iterator it = req._headers.find("Range");
                if(it == req._headers.end())
                {
                    // 普通下载
                    res.SetStatus(200);
                    res.SetHeaders("Content-Type", "application/octet-stream");
                    Filesize(realpath, bodylength);
                    res.SetHeaders("Content-Length", bodylength);
                    res.NormalResponseHeader(clisock);
                    
                    // 边读取边传输
                    int ret = Download(realpath, res, clisock); 
                    if(ret != true)
                    {
                        cout << "Service.hpp/HttpHandle(): Download error" << endl;
                        return false;
                    }
                }
                else 
                {
                    // 断点续传
                    res.SetStatus(206);
                    cout << "当前已进入断点续传!" << endl;
                    string range = it->second;
                    int ret = Rangeload(realpath, range, res, clisock);
                    if(ret != true)
                    {
                        cout << "Service.hpp/HttpHandle(): Rangeload error" << endl;
                        return false;
                    }
                }
            }

            // 文件搜索功能
            if(method == "GET" && req._reqstr.size() != 0)
            {
                cout << "进入文件搜索功能" << endl;
                int flag = 0;
                string reqstr;
                string body;
                string bodylength;
                for(auto& e : req._reqstr)
                {
                    if(e.first == "reqstr")
                    {
                        reqstr = e.second;
                    }
                }

                int ret = FileSearch(reqstr, body, flag, bodylength);    // flag表示文件是否找到(1:找到 / 0:没有)
                if(ret == true)
                {
                    if(flag == 1)
                        res.SetStatus(200);
                    else 
                        res.SetStatus(404);
                    res.SetHeaders("Content-Type", "text/html");
                    res.SetHeaders("Content-Length", bodylength);
                    res.NormalResponseHeader(clisock);
                    
                    res.SetBody(body);
                    res.NormalResponseBody(clisock);

                    return true;
                }
                else 
                {
                    cout << "Service.hpp/HttpHandle(): FileSearch error!" << endl;
                    return false;
                }
            }

            // 文件上传功能
            if(method == "POST")
            {
                cout << "进入文件上传功能" << endl;
                int ret = CGIUpload(req, res);
                if(ret == true)
                {
                    res.NormalResponseHeader(clisock);
                    res.NormalResponseBody(clisock);
                    return true;
                }
                else
                {
                    cout << "Service.hpp/HttpHandle(): CGIUpload error!" << endl;
                    return false;
                }
            }

            return true;
        }

    private:
        static bool ListShow(string& path, string& body, string& bodylength)
        {
            // 组织 HTML 正文信息
            stringstream sshtml; 

            string root = "./DataResource/";
            // 如果path是根目录，那么响应首页
            if(path == root)
            {
                bool ret = ReadFile(sshtml, INDEXHTML);
                if(ret == false)
                {
                    cout << "Service.hpp/ListShow(): ReadFile error!" << endl;
                    return false;
                }
            }
            else 
            {
                bool ret = ReadFile(sshtml, LISTHTML);
                if(ret == false)
                {
                    cout << "Service.hpp/ListShow(): ReadFile error!" << endl;
                    return false;
                }

                // 组织各个文件的结点信息(遍历各个文件: 普通文件 / 目录文件)
                boost::filesystem::directory_iterator begin(path);
                boost::filesystem::directory_iterator end;
                for(; begin != end; ++begin)
                {
                    // 文件名称
                    string filename = begin->path().filename().string();
                    
                    // 文件最后修改时间
                    uint64_t lasttime = boost::filesystem::last_write_time(begin->path());

                    // 提取浏览器的下次请求路径
                    string root = SEVRVICEROOT;
                    string curpath = begin->path().string();
                    string nextreqpath = curpath.substr(root.size());

                    // 组织结点HTML信息
                    if(boost::filesystem::is_directory(begin->path()) == true)
                    {
                        // 当前是目录文件(点击目录后，浏览器再次请求该目录下的资源)
                        sshtml << "<li class='my_fav_list_li'>";
                        sshtml << "<strong ><a  class='my_fav_list_a' href='";
                        sshtml << nextreqpath;
                        sshtml << "' target='_blank' style='text-decoration:underline'>";
                        sshtml << filename << "</a></strong>";
                        sshtml << "<small>文件类型：目录文件 / ";
                        sshtml << "文件大小：未知 / ";
                        sshtml << "文件修改时间：" << lasttime << "</small></li>";
                    }
                    else 
                    {
                        // 当前是普通文件(点击普通文件后，浏览器请求下载该文件)
                        
                        // 文件大小
                        uint64_t filesize = boost::filesystem::file_size(begin->path().string());
                        
                        sshtml << "<li class='my_fav_list_li'>";
                        sshtml << "<strong ><a  class='my_fav_list_a' href='";
                        sshtml << nextreqpath << "' >";
                        sshtml << filename << "</a></strong>";
                        sshtml << "<small>文件类型：普通文件 / ";
                        sshtml << "文件大小：" << filesize / (1024 * 1024) << "MB / ";
                        sshtml << "文件修改时间：" << lasttime << "</small></li>";
                    }
                }

                ret = ReadFile(sshtml, LISTTAIL);
                if(ret == false)
                {
                    cout << "Service.hpp/ListShow(): ReadFile error!" << endl;
                    return false;
                }
            }

            body = sshtml.str();      
            bodylength = to_string(body.size());
            return true;
        }

        static bool CGIUpload(HttpRequest& req, HttpResponse& res)
        {
            // 父子进程管道通信
            int pipefdin[2] = {-1};                      // 父进程向子进程传输请求信息
            int pipefdout[2] = {-1};                     // 子进程向父进程传输响应信息
            int ret = pipe(pipefdin);
            if(ret < 0)
            {
                cout << "Service.hpp/CGIUpload(): pipe create error" << endl;
                return false;
            }
            ret = pipe(pipefdout);
            if(ret < 0)
            {
                cout << "Service.hpp/CGIUpload(): pipe create error" << endl;
                return false;
            }

            // 创建子进程               (fork一类的相关问题?)
            pid_t pid = fork();
            if(pid < 0)
            {
                cerr << "Service.hpp/CGIUpload(): fork error: " << errno << endl;
                return false;
            }
            else if(pid == 0)
            {
                // 设置环境变量，传输请求头部
                for(auto& e : req._headers)
                {
                    ret = setenv(e.first.c_str(), e.second.c_str(), 1);
                    if(ret < 0)
                    {
                        cerr << "Service.hpp/CGIUpload(): setenv error" << endl;
                        continue;
                    }
                }

                // 子进程关闭管道不用的一端
                close(pipefdin[1]);
                close(pipefdout[0]);

                // 由于要程序替换，故重定向文件描述符进行通信
                dup2(pipefdin[0], 1);                                     // 将管道的读取端与标准输出关联起来。
                dup2(pipefdout[1], 0);                                    // 将管道的写入端与标准输入关联起来。

                // 子进程程序替换 
                execlp(CGI, CGI, NULL);                                   // main函数参数来接受传递的参数

                // 关闭重定向后的文件描述符
                close(pipefdin[0]);
                close(pipefdout[1]);
            }

            // 父进程关闭管道不用的一端
            close(pipefdin[0]);
            close(pipefdout[1]);

            // 忽略SIGPIPE信号
            signal(SIGPIPE, SIG_IGN);

            // 向子进程发送正文数据
            size_t wlen = 0;
            while(wlen < req._body.size())
            {
                int ret = write(pipefdin[1], &req._body[0] + wlen, req._body.size() - wlen);
                if(ret < 0)
                {
                    cerr << "Service.hpp/CGIUpload(): write error" << endl;
                    return false;
                }

                wlen += ret;
            }

            // 接受子进程发送的响应信息
            stringstream sshtml;
            char buf[BUFSIZE];
            while(1)
            {
                memset(buf, 0, BUFSIZE);
                int ret = read(pipefdout[0], buf, BUFSIZE);
                if(ret < 0)
                {
                    cerr << "Service.hpp/CGIUpload(): read error" << endl;
                    return false;
                }

                if(ret == 0) break;

                string temp;
                temp.assign(buf, ret);
                sshtml << temp;
            }

            res.SetStatus(200);
            res.SetHeaders("Content-Type", "text/html");
            res.SetHeaders("Content-Length", to_string(sshtml.str().size()));

            res.SetBody(sshtml.str());

            close(pipefdin[1]);
            close(pipefdout[0]);

            return true;
        }

        // 说明：此函数的目的是为了较早计算出文件大小，提升文件下载页面的响应速度。
        static bool Filesize(string& path, string& bodylength)
        {
            // 打开文件
            int fd = open(path.c_str(), O_RDONLY);
            if(fd < 0)
            {
                cout << "Service.hpp/Filesize(): open error!" << endl;
                return false;
            }

            // 获取文件大小
            string len = to_string(lseek(fd, 0, SEEK_END));
            
            // 传出文件大小
            bodylength = len; 

            close(fd);

            return true;
        }

        static bool Download(string& path, HttpResponse& res, Tcpsocket& clisock)
        {
            // 打开文件
            int fd = open(path.c_str(), O_RDONLY);
            if(fd < 0)
            {
                cout << "Service.hpp/Download(): open error!" << endl;
                return false;
            }

            lseek(fd, 0, SEEK_SET);

            // 循环读取文件
            char buf[BUFSIZE];
            while(1)
            {
                memset(buf, 0, BUFSIZE);
                int ret = read(fd, buf, BUFSIZE);
                if(ret < 0)
                {
                    cout << "Service.hpp/Download(): read error!" << endl;
                    return false;
                }
                else if(ret == 0)
                {
                    // 到达文件末尾
                    break;
                }

                string temp;
                temp.assign(buf, ret);
                
                res.SetBody(temp);
                bool retbody = res.NormalResponseBody(clisock);
                if(!retbody) return false;
            }

            close(fd);
            return true;
        }

        // 断点续传
        static bool Rangeload(string& path, string& range, HttpResponse& res, Tcpsocket& clisock)
        {
            // 查找指定字符 (=, -)
            size_t startpos = range.find("=", 0);                    // = 号的下一个位置是范围请求的开始
            size_t endpos = range.find("-", startpos);               // - 号的下一个位置是范围请求的结束
            
            // 切割出数字
            stringstream range1;          // 范围开始
            for(size_t i = startpos + 1; i < endpos; ++i)
                range1 << range[i];

            stringstream range2;          // 范围结束
            for(size_t i = endpos + 1; i < range.size(); ++i)
                range2 << range[i];

            // 打开文件
            int fd = open(path.c_str(), O_RDONLY);
            if(fd < 0)
            {
                cout << "Service.hpp/Rangeload(): open error!" << endl;
                return false;
            }

            // 计算文件大小
            off_t filelength = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);

            // 组织响应头部
            stringstream ss;
            ss << "bytes " << range1.str() << "-" << range2.str() << "/" << filelength;
            res.SetHeaders("Content-Range", ss.str());
            res.SetHeaders("Content-Type", "application/octet-stream");
            res.SetHeaders("Accept-Encoding", "identity");

            // 计算正文长度
            off_t r1, r2;
            range1 >> r1;
            range2 >> r2;
            if(range2.str().size() != 0)
                res.SetHeaders("Content-Length", to_string(r2 - r1));
            else 
                res.SetHeaders("Content-Length", to_string(filelength - r1));


            // 将文件指针偏移到指定的请求位置
            lseek(fd, r1, SEEK_SET);
            
            // 发送响应头部
            res.NormalResponseHeader(clisock);

            // 读取文件数据 (边读取边发送)
            char buf[BUFSIZE];
            while(1)
            {
                memset(buf, 0, BUFSIZE);
                int ret = read(fd, buf, BUFSIZE);
                if(ret < 0)
                {
                    cout << "Service.hpp/Rangeload(): read error!" << endl;
                    return false;
                }
                else if(ret == 0)
                    break;                      // 到达文件末尾
                
                string temp;
                temp.assign(buf, ret);

                res.SetBody(temp);
                bool retbody = res.NormalResponseBody(clisock);
                if(!retbody) return false;
            }

            close(fd);
            return true;
        }

        // 文件搜索 (reqstr: 搜索的文件名)           
        static bool FileSearch(string& reqstr, string& resbody, int& flag, string& bodylength)
        {
            // 查询文件名
            string filename = reqstr;

            // 获取该文件的绝对路径
            string absolutepath;
            boost::filesystem::recursive_directory_iterator begin(SEVRVICEROOT);
            boost::filesystem::recursive_directory_iterator end;
            for(; begin != end; ++begin)
            {
                if(begin->path().stem().string() == filename)
                {
                    absolutepath = begin->path().string();
                }
            }
            boost::filesystem::path fpath(absolutepath);

            // 组织该文件HTML数据
            stringstream sshtml;
            if(absolutepath.size() != 0)
            {
                // 找到了该文件
                flag = 1;
                string root = SEVRVICEROOT;
                string filename = fpath.filename().string();
                uint64_t filesize = boost::filesystem::file_size(absolutepath);
                uint64_t lasttime = boost::filesystem::last_write_time(fpath);
                string nextreqpath = absolutepath.substr(root.size());
                
                bool ret = ReadFile(sshtml, LISTHTML);
                if(ret == false)
                {
                    cout << "Service.hpp/FileSearch(): ReadFile error!" << endl;
                    return false;
                }

                sshtml << "<li class='my_fav_list_li'>";
                sshtml << "<strong ><a  class='my_fav_list_a' href='";
                sshtml << nextreqpath;
                sshtml << "' target='_blank'>";
                sshtml << filename << "</a></strong>";
                sshtml << "<small>文件类型：普通文件 / ";
                sshtml << "文件大小：" << filesize / (1024 * 1024) << "MB / ";
                sshtml << "文件修改时间：" << lasttime << "</small></li>";

                ret = ReadFile(sshtml, LISTTAIL);
                if(ret == false)
                {
                    cout << "Service.hpp/FileSearch(): ReadFile error!" << endl;
                    return false;
                }
            }
            else 
            {
                // 没有找到该文件
                flag = 0;
                bool ret = ReadFile(sshtml, NOTFINDHTML);
                if(ret == false)
                {
                    cout << "Service.hpp/FileSearch(): ReadFile error!" << endl;
                    return false;
                }
            }

            resbody = sshtml.str(); 
            bodylength = to_string(resbody.size());
            return true;
        }

    private:
        Tcpsocket _lissock;       // 监听套接字
        Epoll _ep;                // epoll 模型
        ThreadPool _thpool;       // 线程池
};

void sighandle(const int date)
{
    // 关闭套接字
    close(clisockfd);
}

bool ReadFile(stringstream& ss, const char* filename)
{
    // 打开文件
    int fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        cout << "Service.hpp/ReadFile(): open error!" << endl;
        return false;
    }

    // 设置非阻塞模式
    int flag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);

    // 获取文件大小
    int filelength = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // 循环读取文件内容
    int curread = 0;
    char buf[BUFSIZE];
    while(curread < filelength)
    {
        memset(buf, 0, BUFSIZE);
        int ret = read(fd, buf, 8192);
        if(ret < 0)
        {
            cout << "Service.hpp/ReadFile(): read error!" << endl;
            return false;
        }

        string str;
        str.assign(buf, ret);
        ss << str;
        curread += ret;
    }

    // 关闭文件
    close(fd);

    return true;
}

#endif
