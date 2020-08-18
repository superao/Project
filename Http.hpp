#ifndef __M_HTTP_H__
#define __M_HTTP_H__ 
#include <iostream>
#include <unordered_map>
#include <string>
#include <unistd.h>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "Tcpsocket.hpp"
using namespace std;

#define SEVRVICEROOT "./DataResource"      // 服务器根目录 
#define NOTFINDHTML "./HTML/404page.html"  // 服务器404页面

/******************************************************************
 * HttpRequest类：
 *    成员变量：
 *        string _method;                             // 请求方法
 *        string _path;                               // 请求路径
 *        unordered_map<string, string> _reqstr;      // 查询字符串
 *        unordered_map<string, string> _headers;     // 头部信息
 *        string _body;                               // 正文信息
 *    成员函数：
 *        RequestParse()：请求报文解析，解析完毕后获取头部数据长度
 *        RecvHeader()：获取头部信息 
 *        ParseFirstLine()：解析首行信息
 *        ParseHeaders()：解析头部信息
 *        RecvMessage()：接受正文信息
 *        CheckDate()：输出完整请求报文信息，用于测试
 * ******************************************************************/

// 解析Http报文请求
class HttpRequest
{
    public:
        // 接受正文
        bool RecvMessage(Tcpsocket& sock)
        {
            // 寻找 Content-Lenth 头部
            size_t datelen = 0;
            for(auto& it : _headers)                      // for循环的底层是怎么实现的?
            {
                if(it.first == "Content-Length")
                {
                    datelen = atoi(it.second.c_str());
                }
            }

            // 接受指定长度的数据
            int ret = sock.Recv(_body, datelen);
            if(ret == false)
            {
                cout << "Http.hpp/RecvMessage: socket recv error" << endl;
                return false;
            }

            // 检测所有数据
            CheckDate();

            return true;
        }

    public:
        // 获取头部信息
        bool RecvHeader(Tcpsocket& sock, string& headers)
        {
            // 探测性接受大量数据，检测是否有\r\n\r\n
            while(1)
            {
                // 试探接受数据
                string headtemp;
                int ret = sock.RecvPeek(headtemp);
                if(ret == false)
                {
                    cout << "Http.hpp/RecvHeader(): RecvPeek error" << endl;
                    return false;
                }

                // 查找指定字符串
                size_t pos = headtemp.find("\r\n\r\n", 0);
                // 如果找到字符串末尾还没有找到指定字符
                if(pos == string::npos && headtemp.size() == BUFSIZE)
                {
                    cout << "Http.hpp/RecvHeader(): not find \r\n\r\n" << endl;
                    return false;
                }
                else if(pos != string::npos)
                {
                    // 找到指定字符串后，计算头部长度(包括\r\n\r\n)，正式接受数据
                    size_t datelen = pos + 4;             // \r\n\r\n的长度
                    ret = sock.Recv(headers, datelen);
                    if(ret == false)
                    {
                        cout << "Http.hpp/RecvHeader(): Recv error" << endl;
                        return false;
                    }

                    return true;
                }
            }
        }

    private:
        // 解析首行(Frist 首行字符串)
        bool ParseFirstLine(string& Frist)
        {
            vector<string> Fline;
            boost::split(Fline, Frist, boost::is_any_of(" "), boost::token_compress_on);
            if(Fline.size() != 3)
            {
                cout << "请求首行格式错误！" << endl;
                return false;
            }

            _method = Fline[0];

            size_t pathpos = Fline[1].find("?", 0);
            if(pathpos == string::npos)
            {
                // 没有查询字符串
                _path = Fline[1];
            }
            else 
            {
                // 有查询字符串
                _path = Fline[1].substr(0, pathpos);

                // 处理查询字符串
                string str = Fline[1].substr(pathpos + 1);
                vector<string> findstr;
                boost::split(findstr, str, boost::is_any_of("&"), boost::token_compress_on);
                for(auto& e : findstr)
                {
                    size_t findstrpos = e.find("=", 0);
                    if(findstrpos == string::npos)
                    {
                        cout << "查询字符串格式错误！" << endl;
                        return false;
                    }
                    string key = e.substr(0, findstrpos);
                    string value = e.substr(findstrpos + 1);
                    _reqstr[key] = value;                                   // 哈希表[]是怎么实现的?
                }
            }

            return true;
        }

        // 解析头部(head 单个头部信息)
        bool ParseHeaders(string& head)
        {
            size_t headpos = head.find(": ", 0);
            if(headpos == string::npos)
            {
                cout << "请求头部格式错误！" << endl;
                return false;
            }

            string key = head.substr(0, headpos);
            string value = head.substr(headpos + 1);
            _headers[key] = value;

            return true;
        }

        // 测试请求数据(本函数用于测试)
        bool CheckDate()
        {
            cout << "线程 ID: " << pthread_self() << endl;
            cout << "*********************解析信息********************" << endl;
            cout << "首行解析: " << endl;
            cout << "请求方法: " << _method << endl;
            cout << "请求路径: " << _path << endl;
            cout << "************************************************" << endl;
            cout << "查询字符串: " << endl;
            for(auto& str : _reqstr)
            {
                cout << str.first << "=" << str.second << endl;
            }
            cout << "************************************************" << endl;
            cout << "头部信息: " << endl;
            for(auto& str : _headers)
            {
                cout << str.first << ": " << str.second << endl;
            }      
            cout << "************************************************" << endl;
            cout << "正文信息: \n" << _body << endl;
            cout << "************************************************" << endl;

            return true;
        }

    public:
        // 请求报文解析，接受正文数据
        int RequestParse(string& headers)
        {
            // 保存头部各部分字符串
            vector<string> headlist;                                 
            boost::split(headlist, headers, boost::is_any_of("\r\n"), boost::token_compress_on);

            // 解析首行
            int ret = ParseFirstLine(headlist[0]);
            if(ret == false)
            {
                // 客户端语法错误，请求解析失败
                return 400;
            }

            // 解析头部
            for(int i = 1; i < (int)headlist.size() - 1; ++i)
            {
                ret = ParseHeaders(headlist[i]);
                if(ret == false)
                {
                    // 客户端语法错误，请求解析失败
                    return 400;
                }
            }

            // 检测请求路径是否合法
            bool isexist = false;
            string realpath = SEVRVICEROOT + _path;

            // 检测当前是否是根目录
            if(_path == "/")
                isexist = true;

            // 检测服务器根目录中是否存在该文件
            boost::filesystem::recursive_directory_iterator begin(SEVRVICEROOT);
            boost::filesystem::recursive_directory_iterator end; 
            for(; begin != end; ++begin)
            {
                string temppath = begin->path().string();
                if(temppath == realpath)
                {
                    isexist = true;
                }
            }

            // 服务器中不存在该资源
            if(_method == "GET" && isexist == false) 
                return 404;

            // 解析成功
            return 200;
        }

    public:
        string _method;                             // 请求方法
        string _path;                               // 请求路径
        unordered_map<string, string> _reqstr;      // 查询字符串
        unordered_map<string, string> _headers;     // 头部信息
        string _body;                               // 正文信息
};

/***********************************************************
 * HttpResponse类：
 *    成员变量：
 *           int _status;                                // 响应状态码
 *           string _msg;                                // 状态码表述
 *           unordered_map<string, string> _header;      // 头部信息
 *           string _body;                               // 正文信息   
 *    成员函数：
 *           NormalResponse()：为客户端响应成功界面
 *           ErrorResponse()：为客户端响应错误界面
 *    辅助函数：
 *           SetHeaders()：设置响应头部信息
 *           SetStatus()：设置响应状态码
 *              SetMessage()
 *           SetBody()：设置响应正文信息
 *           SetMessage()：设置响应状态码描述
 * *********************************************************/

// 组织Http报文
class HttpResponse
{
    public:
        // 正常响应头部信息
        bool NormalResponseHeader(Tcpsocket& clisock)
        {
            // 组织响应报文头部信息
            // (说明: 这里设置一些常规的与服务器相关的头部信息，与正文相关的报文信息在组织正文数据的时候设置)
            SetHeaders("Accept-Ranges", "bytes");
            SetHeaders("Etag", "svsd21sdvsd231vsdvs21");
            
            // 构建完整头部信息
            stringstream ss;
            ss << "HTTP/1.1" << " " << _status << " " << _msg << "\r\n";
            for(auto& e : _header)
            {
                ss << e.first << ": " << e.second << "\r\n";
            }
            ss << "\r\n";

            // 发送 HTTP 头部信息
            clisock.Send(ss.str());

            return true;
        }

        // 正常响应正文信息
        bool NormalResponseBody(Tcpsocket& clisock)
        {
            // 发送 HTTP 正文信息
            clisock.Send(_body, _body.size());

            return true;
        }

        // 错误响应(响应400, 404页面)
        bool ErrorResponse(Tcpsocket& clisock)
        {
            stringstream sshtml;
            string bodylength;
            
            // 正文(目的：为了获取正文长度)
            int fd = open(NOTFINDHTML, O_RDONLY);
            if(fd < 0)
            {
                cout << "Http.hpp/ErrorResponse(): open error!" << endl;
                return false;
            }

            bodylength = to_string(lseek(fd, 0, SEEK_END));
            lseek(fd, 0, SEEK_SET);

            stringstream body;
            char buf[BUFSIZE];
            while(1)
            {
                memset(buf, 0, BUFSIZE);
                int ret = read(fd, buf, BUFSIZE);
                if(ret < 0)
                {
                    cout << "Http.hpp/ErrorResponse(): read error!" << endl;
                    return false;
                }
                else if(ret == 0)
                    break;

                string temp;
                temp.assign(buf, ret);
                body << temp;
            }

            close(fd);

            // 首行
            sshtml << "HTTP/1.1" << " " << _status << " " << _msg << "\r\n";

            // 头部
            SetHeaders("Content-Type", "text/html");
            SetHeaders("Content-Length", "bodylength");
            for(auto& e : _header)
            {
                sshtml << e.first << ": " << e.second << "\r\n";
            }
            sshtml << "\r\n";

            sshtml << body.str();

            // 发送报文
            clisock.Send(sshtml.str());

            return true;
        }

        // 设置响应头部信息
        void SetHeaders(const string& key, const string& value)
        {
            _header[key] = value;
        }

        // 设置响应状态码
        void SetStatus(int status)
        {
            // 设置状态码
            _status = status;
            // 设置描述信息
            SetMessage();
        }

        // 设置响应正文信息
        void SetBody(const string& str)
        {
            _body = str;
        }

    private:
        // 设置响应状态码描述
        void SetMessage()
        {
            if(_status == 200)
            {
                _msg = "OK";
            }
            else if(_status == 400)
            {
                _msg = "Bad Request";
            }
            else if(_status == 404)
            {
                _msg = "Not Found";
            }
            else if(_status == 206)
            {
                _msg = "Partial Content";
            }
            else
            {
                _msg = "Unknow";
            }
        }

    private:
        int _status;                                // 响应状态码
        string _msg;                                // 状态码表述
        unordered_map<string, string> _header;      // 头部信息
        string _body;                               // 正文信息   
};

#endif 
