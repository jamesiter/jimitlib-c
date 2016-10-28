#ifndef _NET_UTILS_H_
#define _NET_UTILS_H_
#include "utils.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define MAX_EVENTS 10
#define RECV_BUF_SIZE 1024 * 4
#define MAX_RECV_BUF_SIZE RECV_BUF_SIZE * 8
#define MAX_CONNED 65535
/* 用于判断epoll_event中的fd是否为侦听socket，
 * 4294967296 = 1 << 32 */
#define is_listen_sock_in_epoll(ev) (4294967296 & (ev).data.u64)

typedef struct listen_sock {
    int listen_sock;
    struct sockaddr_in cli_addr;
    socklen_t clilen;
} LISTEN_SOCK;

typedef struct epoll_vars {
    int epollfd;
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
} EPOLL_VARS;

typedef struct recv_buf {
    /* 存放来自网络对端的数据的容器 */
    char buffer[MAX_RECV_BUF_SIZE];
    /* 将要被处理的数据的基地址，将被处理数据的基地址通常会在数据被处理后变为被处理数据的未地址 */
    char *base_ptr; // = buffer; /* Pointer of base address of effective data */
    /* 数据超过边界，有效数据将被，以buffer起始处为基地址，复制回容器;
     * 由于边界是固定的，避免每次判断前进行加法计算，所以创建它时就给它初始好 */
    char *edge_ptr; // = buffer + RECV_BUF_SIZE;
    int e_offset; /* Offset of effective data, begin from base_ptr */
    int n;
} RECV_BUF;

typedef struct conned_obj {
    pthread_mutex_t o_lock;     /* o_lock for object_lock,避免多个线程交叉读写recv_buf和send_buf； */
    int64_t custom_id;          /* 自定义连接对象的标识 */
    int32_t custom_type;        /* 自定义连接对象的类型 */
    int sock_id;
    RECV_BUF recv_buf;
    char msg[BUFSIZ];
    int msg_len;
    int32_t msg_type;       /* 当一个消息经过多个服务器，该标记也许能决定由谁来处理 */
} CONNED_OBJ;

extern CONNED_OBJ *conned_obj_arr[MAX_CONNED];
extern int init_listen_sock(const uint16_t portno, const char *listen_addr, const int type, const int qlen);
extern int init_epoll();
extern void add_listen_sock2epoll(const int epollfd, const int listen_sock);
extern void handle_sock_from_epoll(const int epollfd,
        void (*first_conn)(const int conned_sock),
        void (*after_first_conn)(const int epollfd, const int conned_sock, const int is_logic_msg));
extern void handle_first_connected(const int conned_sock);
extern void handle_after_first_connected(const int epollfd, const int conned_sock, const int is_logic_msg);
extern void handle_disconnect(const int conned_sock);
extern void handle_received_msg_bit_stream(const int conned_sock);
extern void handle_received_data_bit_stream(const int conned_sock);
extern void handle_package_with_data(const int conned_sock);

#endif
