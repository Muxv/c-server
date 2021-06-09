//
// Created by Muxv on 2021/6/9.
//

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
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
*/
// robust io包 emm不知道怎么用（捂脸
#define RIO_BUFSIZE     4096
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



unsigned short PORT = 23333;

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
    char method[64] = {0};
    int num_chars = 0;

//    打印HTTP报文
//    while(1)
//    {
//        num_chars = read_line(client_socket, buf, sizeof(buf));
//        if (num_chars == 0)
//            break;
//        printf("%s\n", buf);
//        memset(buf, 0, sizeof(buf));
//    }

    num_chars = read_line(client_socket, buf, sizeof(buf));
    parse_method(buf, method, sizeof(method));

    if (strcasecmp(method, "GET") == 0)
    {
        printf("\nGET");
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        printf("\nPOST");
    }
    else if (strcasecmp(method, "DELETE") == 0)
    {
        printf("\nDELETE");
    }
    else
    {
        printf("\nmethod");
    }


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

int init_socket()
{
    int httpd = 0;
    // 初始化 套接字
    httpd = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("Socket Error");
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


