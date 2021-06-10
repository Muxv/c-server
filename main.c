#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

/*
struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
};

struct in_addr {
    unsigned long s_addr;  // load with inet_aton()
};

struct sockaddr {
    unsigned short	sa_family;   // address family
    char			sa_data[14]; // protocol address
};

int stat(
　　const char *filename    //文件或者文件夹的路径
　  , struct stat *buf      //获取的信息保存在内存中
);

struct stat
{
    dev_t       st_dev;     ID of device containing file -文件所在设备的ID
    ino_t       st_ino;     inode number -inode节点号
    mode_t      st_mode;    protection -保护模式?
    nlink_t     st_nlink;   number of hard links -链向此文件的连接数(硬连接)
    uid_t       st_uid;     user ID of owner -user id
    gid_t       st_gid;     group ID of owner - group id
    dev_t       st_rdev;    device ID (if special file) 设备号，针对设备文件
    off_t       st_size;    total size, in bytes -文件大小，字节为单位
    blksize_t   st_blksize; blocksize for filesystem I/O -系统块的大小
    blkcnt_t    st_blocks;  number of blocks allocated -文件所占块数
    time_t      st_atime;   time of last access -最近存取时间
    time_t      st_mtime;   time of last modification -最近修改时间
    time_t      st_ctime;   time of last status change -
};
*/
// robust io包 emm不知道怎么用（捂脸
#define RIO_BUFSIZE     4096
#define SERVER_STRING "Server: c-httpd/0.1.0\r\n"
#define EMPTY_MESSAGE " "
#define NOT_FOUND 404
#define OK 200


char STATIC[1024] = {"./cmake-build-default-wsl/static"};
unsigned short PORT = 23333;
time_t timep;
struct tm *p;



typedef struct
{
    int rio_fd;      //与缓冲区绑定的文件描述符的编号
    int rio_cnt;        //缓冲区中还未读取的字节数
    char *rio_bufptr;   //当前下一个未读取字符的地址
    char rio_buf[RIO_BUFSIZE];
}rio_t;

typedef struct { /* represents a pool of connected descriptors */
    int maxfd;        /* largest descriptor in read_set */
    fd_set read_set;  /* set of all active read descriptors */
    fd_set write_set;  /* set of all active read descriptors */
    fd_set ready_set; /* subset of descriptors ready for reading  */
    int nready;       /* number of ready descriptors from select */
    int maxi;         /* highwater index into client array */
    int clientfd[FD_SETSIZE];    /* set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* set of active read buffers */
//    ... 	// ADD WHAT WOULD BE HELPFUL FOR PROJECT1
} pool;



void error_die(const char *sc){
    perror(sc);
    exit(1);
}

int init_socket();

void accept_request(int client_socket);

int read_line(int fd, char *buf, size_t size);

int parse_method(const char *buf, char *method, size_t method_size);

int parse_url(const char *buf, char *url, size_t buf_size, size_t url_size);

void not_found_response(int fd);

void http_log(char* method, char* API, int code, char* appendix);





int main() {
    // 服务端 套接字

    setbuf(stdout, 0);

    int httpd_sock;
    // 用于记录 来访的客户端的套接字地址
    int client_sock;
    struct sockaddr_in client_name;
    int client_name_len = sizeof(client_name);

    httpd_sock = init_socket();
    printf("HTTPD running on %d\n", PORT);

    while(1)
    {
        client_sock = accept(httpd_sock,
                             (struct sockaddr *)&client_name, (unsigned int *)&client_name_len);
        if (client_sock == -1){ error_die("Accept Error"); break; }

        accept_request(client_sock);
    }

    close(httpd_sock);

}

void accept_request(int client_socket)
{
    // 读取请求的第一行
    char buf[1024] = {0};
    // 请求方法
    char method[64] = {0};
    // 请求URL
    char url[1024] = {0};
    // 本地对应的位置
    char path[1024] = {0};
    int num_chars = 0;
    // _stat 函数用来获取指定路径的文件或者文件夹的信息
    struct stat st;


//    打印HTTP报文
//    while(1)
//    {
//        num_chars = read_line(client_socket, buf, sizeof(buf));
//        if (num_chars == 0)
//            break;
//        printf("%s\n", buf);
//        memset(buf, 0, sizeof(buf));
//    }

    // 读取第一行 解析方法和路径
    num_chars = read_line(client_socket, buf, sizeof(buf));
    parse_method(buf, method, sizeof(method));
    parse_url(buf, url, sizeof(buf), sizeof(url));

    // GET 返回200+文件 或者返回404
    if (strcasecmp(method, "GET") == 0)
    {
        sprintf(path, "%s%s", STATIC, url);
        // url 以 / 结尾默认读取index.html
        if (path[strlen(path) - 1] == '/')
            strcat(path, "index.html");
        // 找不到文件
        if (stat(path, &st) == -1)
        {
            // 丢弃http头
            while ((num_chars > 0) && strcmp("\n", buf) != 0)
                num_chars = read_line(client_socket, buf, sizeof(buf));
            not_found_response(client_socket);
            http_log(method, url, NOT_FOUND, EMPTY_MESSAGE);

        }
    }
//    else if (strcasecmp(method, "POST") == 0) printf("\nPOST");
//    else if (strcasecmp(method, "DELETE") == 0) printf("\nDELETE");
//    else printf("\nmethod");

    close(client_socket);
}

int read_line(int fd, char *buf, size_t size)
{
    // 读取的字节数
    int count = 0;
    char c = '\0';
    int n;
    // < size - 1 是为了给\0留位置
    while((count < size - 1) && (c != '\n'))
    {
        n = recv(fd, &c, 1, 0);
        if (n > 0)
        {
            // 遇到\r 需要再读掉下一个字节 可能是\n
            if (c == '\r')
            {
                // 使用 MSG_PEEK 标志使下一次读取依然可以得到这次读取的内容，可认为接收窗口不滑动
                n = recv(fd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(fd, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[count] = c;
            count++;
        }
        else
            break;
    }
    buf[count] = '\0';



    return count;
}

/*
 * 读取HTTP首行内容中的方法
 * 可能输入为
 * GET / HTTP/1.1
 * 方法在第一行的空格前
 * 返回读到的方法的字节长度
 */
int parse_method(const char *buf, char *method, size_t method_size)
{
    int i = 0;
    int j = 0;
    while(buf[i] != ' ' && j < method_size - 1)
    {
        method[j++] = buf[i++];
    }
    method[j] = '\0';
    return j;
}

int parse_url(const char *buf, char *url, size_t buf_size, size_t url_size)
{
    int i = 0;
    int j = 0;
    // 跳过方法和中间的空格
    while(buf[i] != ' ')
        i++;
    while(buf[i] == ' ')
        i++;
    while(buf[i] != ' ' && j < url_size - 1 && i < buf_size)
    {
        url[j++] = buf[i++];
    }
    url[i] = '\0';

    return j;







}

int init_socket()
{
    int httpd = 0;
    // 初始化 套接字
    httpd = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("Socket Error");

    // 设置后关闭不会占用端口
    int opt = 1;
    setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 定义本机套接字的端口地址
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(PORT);
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 端口绑定到地址
    if (bind(httpd,  (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
        error_die("Bind Error");
    // 开始监听
    if (listen(httpd, 5) < 0)
        error_die("Listen error");

    return httpd;
}

void not_found_response(int fd)
{
    char buf[1024];

    /* 404 页面 */
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(fd, buf, strlen(buf), 0);
    /*服务器信息*/
    sprintf(buf, SERVER_STRING);
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(fd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(fd, buf, strlen(buf), 0);
}


void http_log(char* method, char* url, int code, char* appendix)
{
//    [24/08/2018 20:41:07] "GET / HTTP/1.1" 200 -
    char output[1024] = {0};
    time_t time_p;
    struct tm *p;
    time(&time_p);
    p = gmtime(&time_p);
//    printf("%d-", 1900 + p->tm_year);      /*获取当前年份,从1900开始，所以要加1900*/
//    printf("%d-", 1 + p->tm_mon);          /*获取当前月份,范围是0-11,所以要加1*/
//    printf("%d ", p -> tm_mday);           /*获取当前月份日数,范围是1-31*/
//    printf("%d:", 8 + p->tm_hour);         /*获取当前时,这里获取西方的时间,刚好相差八个小时*/
//    printf("%d:", p -> tm_min);            /*获取当前分*/
//    printf("%d\n", p -> tm_sec);           /*获取当前秒*/
    sprintf(output, "[%d/%d/%d %d:%d:%d] \"%s %s HTTP/1.1\" %d -%s\n",
            p -> tm_mday, 1 + p->tm_mon, 1900 + p->tm_year,
            8 + p -> tm_hour, p -> tm_min, p -> tm_sec,
            method, url, code, appendix);
    printf("%s", output);
}