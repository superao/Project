// 文件上传
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

#define BUFSIZE 10240                               // 缓冲区大小
#define SEVRVICEROOT "./DataResource/"              // 服务器根目录
#define SUCCESSHTML "./HTML/uploadsuccess.html"     // 文件上传成功HTML界面

// boundary 字段
class Boundary
{
    public:
        string _data;              // 文件数据
        string _name;              // input 请求字段
        string _filename;          // 文件名称
};

// 在所有环境变量中，解析出所需数据
bool EnvParse(vector<string>& envlist, string& boundary, string& len)
{
    // 寻找所需环境变量
    string boundaryenv;
    string lenenv;
    for(auto& e : envlist)
    {
        // Content-Type
        if(e.find("Content-Type") != string::npos)
        {
            boundaryenv = e;
        }
        // Content-Length
        if(e.find("Content-Length") != string::npos)
        {
            lenenv = e;
        }
    }

    // 解析环境变量，获取所需数据
    string btemp = "boundary=";
    size_t bpos = boundaryenv.find(btemp, 0);
    if(bpos == string::npos)
    {
        cerr << "Cannot find the specified character! boundary=" << endl;
        return false;
    }
    boundary = boundaryenv.substr(bpos + btemp.size());

    string ltemp = "= ";
    size_t lpos = lenenv.find(ltemp, 0);
    if(lpos == string::npos)
    {
        cerr << "Cannot find the specified character! = " << endl;
        return false;
    }
    len = lenenv.substr(lpos + ltemp.size());

    return true;
}

// 解析正文中的头部信息(name, filename)
bool HeadParse(string& header, string& name, string& filename)
{
    // 分割头部信息
    vector<string> hlist;
    boost::split(hlist, header, boost::is_any_of("\r\n"), boost::token_compress_on);

    // 遍历查找 Content-Disposition
    string contentdisposition;
    for(auto& e : hlist)
    {
        if(e.find("Content-Disposition", 0) != string::npos)
        {
            contentdisposition = e;
        }
    }

    // 查找 name, filename
    string findstr = "\"";
    string findstr1 = "name=\"";
    string findstr2 = "filename=\"";
    // name
    size_t pos1 = header.find(findstr1, 0);
    size_t pos2 = header.find(findstr, pos1 + findstr1.size());
    if(pos1 == string::npos || pos2 == string::npos)
    {
        cerr << "数据格式错误" << endl;
        return false;
    }
    name = header.substr(pos1 + findstr1.size(), pos2 - pos1 - findstr1.size());

    // filename
    pos1 = header.find(findstr2, 0);
    pos2 = header.find(findstr, pos1 + findstr2.size());
    if(pos1 != string::npos && pos2 != string::npos)        // filename 在有些数据段中有可能不存在
    {
        filename = header.substr(pos1 + findstr2.size(), pos2 - pos1 - findstr2.size());
    }

    return true;
}

// 解析正文 boundary 中的文件数据
bool BoundaryParse(string& body, string& boundary, string& fname, string& textdata)
{
    // 储存分段信息
    vector<Boundary> blist;

    // 规定特殊字符
    size_t pos = 0, nextpos = 0;                 // pos 指向数据起始位置，nextpos 指向数据终止位置
    string flag = "--";
    string linebreak = "\r\n";
    string doublelinebreak = "\r\n\r\n";
    string fboundary = flag + boundary;
    string lboundary = fboundary + flag + linebreak;

    while(pos < body.size())
    {
        pos = body.find(fboundary, pos);
        nextpos = body.find(linebreak, pos);
        if(pos == string::npos || nextpos == string::npos)
        {
            cerr << "数据格式错误" << endl;
            return false;
        }

        // 储存每个头部信息
        string head;
        pos = nextpos + linebreak.size();
        nextpos = body.find(doublelinebreak, pos);
        head = body.substr(pos, nextpos - pos);

        // 储存每个上传数据
        string data;
        pos = nextpos + doublelinebreak.size();
        nextpos = body.find(fboundary, pos);            
        data = body.substr(pos, nextpos - pos - linebreak.size());

        // 解析本分段头部信息
        string name, filename;
        int ret = HeadParse(head, name, filename);
        if(ret == false)
        {
            cerr << "HeadParse error" << endl;
            return false;
        }

        // 进行储存
        Boundary bo;
        bo._name = name;
        bo._filename = filename;
        bo._data = data;
        blist.push_back(bo);

        // pos指向下一个分段头部
        pos = nextpos;
        string breakstr = body.substr(pos);
        if(breakstr == lboundary)
        {
            break;
        }
    }

    // 遍历组织信息
    stringstream ss;
    for(auto& e : blist)
    {
        if(e._filename.size() != 0 && e._name == "fileload")
        {
            fname = e._filename;
            ss << e._data;
        }
    }
    textdata = ss.str();                     

    return true;
}

// 储存文件
bool SaveFile(string& path, string& body)
{
    // 创建文件流对象(写)
    ofstream fout;
    fout.open(path, ofstream::out | ofstream::trunc );
    if(!fout.is_open())
    {
        cerr << "open file error" << endl;
        return false;
    }

    // 写入文件
    fout.write(body.c_str(), body.size());
    if(!fout.good())
    {
        cerr << "write date error" << endl;
        return false;
    }

    return true;
}

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

    return true;
}

int main(int argc, char* argv[], char* env[]) 
{
    // 接受所有环境变量 (main函数参数 / getenv())
    vector<string> envlist;
    for(int i = 0; env[i] != NULL; ++i)
    {
        string temp;
        temp.assign(env[i]);
        envlist.push_back(temp);
    }

    // 从环境变量中解析出以下信息
    string length;                      // 正文长度  
    string boundary;                    // boundary字符
    int ret = EnvParse(envlist, boundary, length);
    if(ret == false)
    {
        cerr << "upload.cpp: EnvParse error!" << endl;
        return -1;
    }

    // 测试所解析出的头部信息
    cerr << "length:" << length << endl;
    cerr << "boundary:" << boundary << endl;

    // 利用解析出的正文长度，循环接受正文数据
    cerr << "子进程开始接受数据!" << endl; 
    int datelen = atoi(length.c_str());
    int rlen = 0;
    string body;
    body.resize(datelen);
    while(rlen < datelen)
    {
        // string temp;               这种临时变量的方式非常浪费时间，最后还需要一次拷贝，这种很浪费时间。
        // temp.resize(datelen);
        ret = read(1, &body[0] + rlen, datelen - rlen);
        if(ret < 0)
        {
            cerr << "read error" << endl;
            return -1;
        }

        rlen += ret;
    }
    cerr << "子进程接受数据完毕!" << endl;
    cerr << "-------------正文数据为： " << endl;
    cerr << body;

    // 解析正文数据
    string fname;
    string textdata;
    ret = BoundaryParse(body, boundary, fname, textdata);
    if(ret == false)
    {
        cerr << "BoundaryParse error" << endl;
        return -1;
    }

    // 储存文件   (注：fname 只是一个文件名，需要加上服务器根目录)
    string fullfilename = SEVRVICEROOT + fname;
    ret = SaveFile(fullfilename, textdata);
    if(ret == false)
    {
        cerr << "SaveFile error" << endl;
        return -1;
    }

    // 向父进程发送响应信息
    stringstream successhtml;
    ReadFile(successhtml, SUCCESSHTML);
    write(0, successhtml.str().c_str(), successhtml.str().size());

    return 0;
}
