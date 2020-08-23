#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

#define BUFSIZE 10240                               // 文件接受缓冲区
#define SAVEROOT "./DataResource/AllData/"          // 储存文件的根目录
#define SUCCESSHTML "./HTML/uploadsuccess.html"     // 文件上传成功HTML界面

// 解析环境变量 
bool EnvParse(vector<string>& envlist, string& boundary, string& len)
{
    // 寻找所需环境变量
    string boundaryenv;
    string lenenv;
    for(auto& e : envlist)
    {
        // Content-Type
        if(e.find("Content-Type") != string::npos)
            boundaryenv = e;
        
        // Content-Length
        if(e.find("Content-Length") != string::npos)
            lenenv = e;
    }

    // 解析各个字段
    string bdystr = "boundary=";
    size_t bpos = boundaryenv.find(bdystr, 0);
    if(bpos == string::npos)
    {
        cerr << "upload.cpp/EnvParse(): Cannot find the boundary=" << endl;
        return false;
    }
    boundary = boundaryenv.substr(bpos + bdystr.size());

    string lenstr = "=";
    size_t lpos = lenenv.find(lenstr, 0);
    if(lpos == string::npos)
    {
        cerr << "upload.cpp/EnvParse(): Cannot find the = " << endl;
        return false;
    }
    len = lenenv.substr(lpos + lenstr.size());

    return true;
}

// 解析头部信息 -> filename
bool ParseHead(string& headdata, string& filename)
{
    // 定义查找的特定字符
    string findstr1 = "filename=\"";
    string findstr2 = "\"";

    // 查找filename的值
    size_t startpos = headdata.find(findstr1, 0);
    size_t endpos = headdata.find(findstr2, startpos + findstr1.size());
    if(startpos == string::npos || endpos == string::npos)
        return false;
                                                           // (endpos - 1) - (startpos + findstr1.size()) + 1
    filename = headdata.substr(startpos + findstr1.size(), endpos - (startpos + findstr1.size()));

    return true;
}

// 从完整正文信息中解析出文件数据 -> (本质上第一部分数据就是上传文件的数据)
bool BoundaryParse(string& body, string& boundary, string& filename, string& filedata, bool& iscontinue)
{
    // 规定特殊字符
    string flag = "--";
    string flag1 = "\r\n";
    string firststartflag = flag + boundary;                    // 第一部分开始标记 
    string firstendflag = "\r\n\r\n";                           // 第一部分头部结束标记 
    string fileendflag = flag1 + firststartflag;                // 第一部分结束标记

    size_t startpos = body.find(firststartflag);
    size_t endpos = body.find(firstendflag, startpos);
    size_t fileendpos = body.find(fileendflag);
    
    // 检测当前数据是否既包含了第一部分头部信息又包含第一部分结束标记                   (防止结束标记中包含了开始标记)
    if(startpos != string::npos && endpos !=  string::npos && fileendpos != string::npos && fileendpos > startpos)
    {
        // 解析头部信息 -> filename
        string headdata = body.substr(startpos, endpos - 1 - startpos + 1);
        ParseHead(headdata, filename);
        
        // 切出"中间"的数据
        filedata = body.substr(endpos + firstendflag.size(), fileendpos - 1 - (endpos + firstendflag.size()) + 1);
        
        return true;
    }
    
    // 检测当前数据中是否存在第一部分头部信息
    if(startpos != string::npos && endpos !=  string::npos && fileendpos == string::npos)
    {
        // 解析头部信息 -> filename
        string headdata = body.substr(startpos, endpos - 1 - startpos + 1);
        ParseHead(headdata, filename);

        // 切出第一部分头部信息之后的全部数据
        filedata = body.substr(endpos + firstendflag.size());

        return true;
    }

    // 检测当前数据中是否存在第一部分结束标记
    if(fileendpos != string::npos)
    {
        // 切出第一部分结束标记之前的全部数据
        filedata = body.substr(0, fileendpos - 1 - 0 + 1);

        // 已到达文件数据末尾，不需要再解析了
        iscontinue = false;
        
        return true;
    }

    // 当前数据既没有第一部分头部信息与没有第一部分结束标记，即纯文件数据
    filedata = body;
    
    return true;
}

// 写文件
bool SaveFile(const string& filename, const string& body)
{
    // 文件完整路径
    string filepath = SAVEROOT + filename;

    // 创建文件
    int filefd = open(filepath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0664);
    if(filefd < 0)
    {
        cerr << "upload.cpp/SaveFile(): open error!" << endl;
        perror("open error!");
        return false;
    }

    // 设置非阻塞模式
    int flag = fcntl(filefd, F_GETFL);
    fcntl(filefd, F_SETFL, flag | O_NONBLOCK);

    // 写入数据
    size_t bodylen = body.size();
    size_t curwrite = 0;
    while(curwrite < bodylen)
    {
        int ret = write(filefd, &body[0] + curwrite, bodylen - curwrite);
        if(ret < 0)
        {
            if(ret == EAGAIN)
                continue;
            else 
            {
                cerr << "upload.cpp/SaveFile(): write error!" << endl;
                perror("write error!");
                return false;
            }
        }

        curwrite += ret;
    }

    // 关闭文件
    close(filefd);

    return true;
}

// 读文件
bool ReadFile(stringstream& ss, const char* filename)
{
    // 打开文件
    int fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        cout << "upload.cpp/ReadFile(): open error!" << endl;
        perror("open error!");
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
            cout << "upload.cpp/ReadFile(): read error!" << endl;
            perror("read error!");
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

uint64_t stringtouint64(string strnum)
{
    uint64_t num = 0;
    uint64_t lastnum = 0;
    for(int i = strnum.size() - 1, j = 0; i >= 0; --i, ++j)
    {
        num = (strnum[i] - '0') * pow(10, j) + lastnum; 
        lastnum = num;
    }

    return num;
}

int main(int argc, char* argv[], char* env[]) 
{
    // 接受环境变量 (main函数参数 / getenv())
    vector<string> envlist;
    for(int i = 0; env[i] != NULL; ++i)
    {
        string temp;
        temp.assign(env[i]);
        envlist.push_back(temp);
    }

    // 解析环境变量 
    string length;                      // 正文长度  
    string boundary;                    // boundary字符
    int ret = EnvParse(envlist, boundary, length);
    if(ret == false)
    {
        cerr << "upload.cpp: EnvParse error!" << endl;
        return -1;
    }

    // 接受正文数据 (边接受边解析边储存) 
    uint64_t datalen = stringtouint64(length);
    string filename;
    char buf[BUFSIZE];
    uint64_t rlen = 0;
    bool iscontinue = true;
    while(rlen < datalen)
    {
        memset(buf, 0, BUFSIZE);
        ret = read(1, buf, BUFSIZE);
        if(ret < 0)
        {
            cerr << "upload.cpp: read error" << endl;
            perror("read error!");
            return -1;
        }

        string temp;
        temp.assign(buf, ret);
        rlen += ret;

        // 解析正文数据
        string filedata;
        BoundaryParse(temp, boundary, filename, filedata, iscontinue);

        // 将数据存储到文件
        cerr << "文件名称：" << filename << endl;
        SaveFile(filename, filedata);

        // 文件数据已全部读完
        if(!iscontinue)
            break;
    }

    // 发送响应信息
    stringstream successhtml;
    ReadFile(successhtml, SUCCESSHTML);
    write(0, successhtml.str().c_str(), successhtml.str().size());

    close(0);
    close(1);

    return 0;
}
