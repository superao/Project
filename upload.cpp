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

// boundary区域
class Boundary
{
    public:
        string _data;              // 文件数据
        string _name;              // input请求字段
        string _filename;          // 文件名称
};

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

    string lenstr = "= ";
    size_t lpos = lenenv.find(lenstr, 0);
    if(lpos == string::npos)
    {
        cerr << "upload.cpp/EnvParse(): Cannot find the = " << endl;
        return false;
    }
    len = lenenv.substr(lpos + lenstr.size());

    return true;
}

// 解析本区域的头部信息 -> name, filename
bool ParseHead(string& headdata, Boundary& temp)
{
    // 定义查找的特定字符
    string findstr1 = "name=\"";
    string findstr2 = "filename=\"";
    string findstr3 = "\"";

    // 查找name的值
    size_t startpos = headdata.find(findstr1, 0);
    size_t endpos = headdata.find(findstr3, startpos + findstr1.size());
    if(startpos == string::npos || endpos == string::npos)
        return false;

    string name = headdata.substr(startpos + findstr1.size(), endpos - (startpos + findstr1.size()));
    temp._name = name;
    
    // 查找filename的值
    startpos = headdata.find(findstr2, 0);
    endpos = headdata.find(findstr3, startpos + findstr2.size());
    if(startpos == string::npos || endpos == string::npos)
        return true;

    string filename = headdata.substr(startpos + findstr2.size(), endpos - (startpos + findstr2.size()));
    temp._filename = filename;

    return true;
}

// 解析本区域相关数据
bool ParsePartData(string& partdata, Boundary& temp)
{
    // 当前区域头部信息
    string endflag = "\r\n\r\n";
    size_t pos = partdata.find(endflag, 0);
    string headdata = partdata.substr(0, pos - 0 + 1);
    stringstream ss;
    ss << "     ++++     " << headdata << "     +++++      " << endl;
    cerr << ss.str();

    // 解析头部信息
    ParseHead(headdata, temp);

    // 切割文件数据
    string filedata;
    size_t startfilepos = pos + endflag.size();
    size_t endfilepos = partdata.size() - 2;         // "\r\n"
    filedata = partdata.substr(startfilepos, endfilepos - startfilepos + 1);
    temp._data = filedata;

    return true;
}

// 从正文信息中解析出文件数据
bool BoundaryParse(const string& body, const string& boundary, string& filename, stringstream& filedata)
{
    // 定义特殊字符
    size_t startpos = 0, endpos = 0;
    string flag = "--";
    string fboundary = flag + boundary;                             // 每个区域的起始标记
    string lboundary = fboundary + flag;                            // 正文数据的结束标记 (其中包含着起始标记)
    
    // 将正文划分为多个区域
    vector<Boundary> boundarylist;
    while(1)
    {
        startpos = body.find(fboundary, startpos);                  // 当前区域的起始位置
        endpos = body.find(fboundary, startpos + 1) - 1;            // 当前区域的终止位置
        if(endpos == string::npos - 1)
        {
            endpos = body.find(lboundary, startpos + 1) - 1;
            if(endpos == string::npos - 1)
                break;
        }

        // 切割当前区域
        string partdata;
        partdata = body.substr(startpos, endpos - startpos + 1);
        cerr << "当前区域为: " << endl;
        cerr << partdata << endl;

        // 解析当前区域
        Boundary temp;
        ParsePartData(partdata, temp);
        boundarylist.push_back(temp);

        startpos = endpos;
    }

    // 遍历组织信息
    for(auto& e : boundarylist)
    {
        if(e._filename.size() != 0 && e._name == "fileload")
        {
            filename = e._filename;
            filedata << e._data;
        }
    }

    return true;
}

// 写文件
bool SaveFile(const string& filename, const string& body)
{
    // 文件完整路径
    string filepath = SAVEROOT + filename;

    // 创建文件
    int filefd = open(filepath.c_str(), O_RDWR | O_CREAT, 0664);
    if(filefd < 0)
    {
        cerr << "upload.cpp/SaveFile(): open error!" << endl;
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

    // 接受正文数据 
    stringstream bodydata;
    uint64_t datalen = stringtouint64(length);
    char buf[BUFSIZE];
    uint64_t rlen = 0;
    while(rlen < datalen)
    {
        memset(buf, 0, BUFSIZE);
        ret = read(1, buf, BUFSIZE);
        if(ret < 0)
        {
            cerr << "upload.cpp: read error" << endl;
            return -1;
        }

        string temp;
        temp.assign(buf, ret);
        bodydata << temp;
        rlen += ret;
    }

    // 解析正文数据
    string filename;
    stringstream filedata;
    ret = BoundaryParse(bodydata.str(), boundary, filename, filedata);
    if(ret == false)
    {
        cerr << "upload.cpp: BoundaryParse error" << endl;
        return -1;
    }

    // 储存文件 
    ret = SaveFile(filename, filedata.str());
    if(ret == false)
    {
        cerr << "upload.cpp: SaveFile error" << endl;
        return -1;
    }

    // 发送响应信息
    stringstream successhtml;
    ReadFile(successhtml, SUCCESSHTML);
    write(0, successhtml.str().c_str(), successhtml.str().size());

    close(0);
    close(1);

    return 0;
}
