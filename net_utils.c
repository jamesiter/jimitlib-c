#include "net_utils.h"

CONNED_OBJ *conned_obj_arr[MAX_CONNED];

int init_listen_sock(const uint16_t portno, const char *listen_addr, const int type, const int qlen) {
    int bFlag = true;
    struct sockaddr_in serv_addr;

    int listen_sock = socket(AF_INET, type, 0);
    if (0 > listen_sock) {
        err_sys("ERROR: error by socket");
    }
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &bFlag, sizeof(bFlag));
    /* 由于serv_addr所占空间很小，所以bzero速度很快 */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(listen_addr);
    serv_addr.sin_port = htons(portno);
    if (0 > bind(listen_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) {
        err_sys("ERROR: error by bind");
    }
    if (SOCK_STREAM == type || SOCK_SEQPACKET == type) {
        if (0 > listen(listen_sock, qlen)) {
            err_sys("ERROR: error by listen");
        }
    }
    return listen_sock;
}

int init_epoll() {
    int epollfd = epoll_create(MAX_EVENTS);
    if (-1 == epollfd) {
        err_sys("ERROR: error by epoll_create");
    }
    return epollfd;
}

void add_listen_sock2epoll(const int epollfd, const int listen_sock) {
    struct epoll_event ev;
    /* 存放fd的空间大小为32位，利用联合中的64位高位来指定socket类型，
     * 第33位若为1，则表示该socket为侦听socket */
    ev.data.u64 = 1;
    ev.data.u64 <<= 32;
    /* 实际应用中，程序只会从侦听socket读，不会去写，
     * 故只通过EPOLLIN关联它的读事件 */
    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev)) {
        err_sys("ERROR: error by epoll_ctl add listen_sock");
    }
}

void handle_sock_from_epoll(const int epollfd,
        void (*first_conn)(const int conned_sock),
        void (*after_first_conn)(const int epollfd, const int conned_sock, const int is_logic_msg)) {
    int conned_sock, nfds, i;  /* nfds for notify file descriptors */
    LISTEN_SOCK l_s;
    EPOLL_VARS epoll_vars;
    l_s.clilen = sizeof(l_s.cli_addr);
    epoll_vars.epollfd = epollfd;
    while(true) {
        nfds = epoll_wait(epoll_vars.epollfd, epoll_vars.events, MAX_EVENTS, -1);
        if (-1 == nfds) {
            err_sys("ERROR: error by epoll_wait");
        }
        for (i = 0; i < nfds; ++i) {
            if (is_listen_sock_in_epoll(epoll_vars.events[i])) {
                conned_sock = accept(epoll_vars.events[i].data.fd, (struct sockaddr *) &l_s.cli_addr, &l_s.clilen);
                if (-1 == conned_sock) {
                    err_sys("ERROR: By accept");
                }
                setnonblocking(conned_sock);
                /* 对于和客户端直接连接的socket，以边沿触发模式关联可读事件到epoll */
                epoll_vars.ev.events = EPOLLIN | EPOLLET;
                /* 避免高32位随机值 */
                epoll_vars.ev.data.u64 = 0;
                epoll_vars.ev.data.fd = conned_sock;
                if (-1 != epoll_ctl(epoll_vars.epollfd, EPOLL_CTL_ADD, conned_sock, &epoll_vars.ev)) {
                    first_conn(conned_sock);
                } else {
                    err_sys("ERROR: By epoll_ctl conned_sock");
                }
            } else {
                after_first_conn(epollfd, epoll_vars.events[i].data.fd, false);
            }
        }
    }
    /* 程序结束时，会由内核关闭其打开的相关文件描述符，所以这里无需自寻烦恼 */
}

void handle_first_connected(const int conned_sock) {
    conned_obj_arr[conned_sock] = malloc(sizeof(CONNED_OBJ));
    conned_obj_arr[conned_sock]->sock_id = conned_sock;
    /* 初始化将要被处理的数据的基地址 */
    conned_obj_arr[conned_sock]->recv_buf.base_ptr = conned_obj_arr[conned_sock]->recv_buf.buffer;
    /* 数据超过边界，有效数据将被，以buffer起始处为基地址，复制回容器;
     * 由于边界是固定的，避免每次判断前进行加法计算，所以创建它时就给它初始好 */
    conned_obj_arr[conned_sock]->recv_buf.edge_ptr = conned_obj_arr[conned_sock]->recv_buf.buffer + RECV_BUF_SIZE;
    conned_obj_arr[conned_sock]->recv_buf.e_offset = 0;
    conned_obj_arr[conned_sock]->recv_buf.n = 0;
    pthread_mutex_init(&conned_obj_arr[conned_sock]->o_lock, NULL);
    clogger("Have a server connected!");
}

void handle_after_first_connected(const int epollfd, const int conned_sock, const int is_logic_msg) {
    while(true) {
        conned_obj_arr[conned_sock]->recv_buf.n = read(conned_sock,
                conned_obj_arr[conned_sock]->recv_buf.base_ptr + conned_obj_arr[conned_sock]->recv_buf.e_offset,
                RECV_BUF_SIZE);
        if (0 > conned_obj_arr[conned_sock]->recv_buf.n) {
            // if (EAGAIN == errno) {
            // }
            return;
        }
        conned_obj_arr[conned_sock]->recv_buf.e_offset += conned_obj_arr[conned_sock]->recv_buf.n;
        /* 若新接收到的数据超出边界，则复制其到buffer起始处，base_ptr也置为初始值 */
        if (conned_obj_arr[conned_sock]->recv_buf.edge_ptr <
                conned_obj_arr[conned_sock]->recv_buf.base_ptr + conned_obj_arr[conned_sock]->recv_buf.e_offset) {
            memcpy(conned_obj_arr[conned_sock]->recv_buf.buffer, conned_obj_arr[conned_sock]->recv_buf.base_ptr,
                conned_obj_arr[conned_sock]->recv_buf.e_offset);
            conned_obj_arr[conned_sock]->recv_buf.base_ptr = conned_obj_arr[conned_sock]->recv_buf.buffer;
        }
        if (0 < conned_obj_arr[conned_sock]->recv_buf.n) {
            if (is_logic_msg) {
                handle_received_msg_bit_stream(conned_sock);
            } else {
                handle_received_data_bit_stream(conned_sock);
            }
        // } else if (0 == conned_obj_arr[conned_sock]->recv_buf.n) {
        } else {
            if (-1 != epoll_ctl(epollfd, EPOLL_CTL_DEL, conned_sock, NULL)) {
                handle_disconnect(conned_sock);
            } else {
                err_sys("ERROR: error by epoll_ctl delete conned_sock");
            }
            close(conned_sock);
            return;
        }
    }
}

void handle_disconnect(const int conned_sock) {
    // printf("Disconnect sock: %d\n", conned_sock);
    pthread_mutex_destroy(&conned_obj_arr[conned_sock]->o_lock);
    free(conned_obj_arr[conned_sock]);
    conned_obj_arr[conned_sock] = NULL;
}

void handle_received_msg_bit_stream(const int conned_sock) {
    /*
     * |---------------------------obj_msg----------------------|
     * +-------------------+----------------+-------------------+
     * | <uint32_t length> | [int32_t type] |    <real data>    |
     * +---------+---------+----------------+-------------------+
     *           |                          ^                   ^
     *           |                          |-------length------|
     *           |                                    ^
     *           +------------------------------------|
     */
    while (true) {  //处理尽recv_buf里面所接收到完整的msg。
        /* 判断recv_buf是否满足协议包最小长度；
         * 判断recv_buf的总大小是否满足一个完整(包含长度和类型)逻辑包的大小；
         * 两者都满足则继续向下，否则返回； */
        if (conned_obj_arr[conned_sock]->recv_buf.e_offset < sizeof(uint32_t) * 2 ||
                conned_obj_arr[conned_sock]->recv_buf.e_offset <
                *(uint32_t *) conned_obj_arr[conned_sock]->recv_buf.base_ptr + sizeof(uint32_t) * 2) {
                /*                                         msg长度     存放msg长度值、类型值的数据类型长度 */
            return;
        }
        /* 获取消息长度 */
        conned_obj_arr[conned_sock]->msg_len = *(uint32_t *) conned_obj_arr[conned_sock]->recv_buf.base_ptr;
        /* 收取明显错误的包，直接断开 */
        if (conned_obj_arr[conned_sock]->msg_len > MAX_RECV_BUF_SIZE) {
            err_msg("ERROR: Too big msg_len by %d\n", conned_sock);
            handle_disconnect(conned_sock);
        }
        /* 逃过存放msg长度的地址空间 */
        conned_obj_arr[conned_sock]->recv_buf.base_ptr += sizeof(uint32_t);
        /* 有效数据长度减去msg长度的地址空间长度 */
        conned_obj_arr[conned_sock]->recv_buf.e_offset -= sizeof(uint32_t);
        /* 获取消息类型 */
        conned_obj_arr[conned_sock]->msg_type = *(uint32_t *) conned_obj_arr[conned_sock]->recv_buf.base_ptr;
        /* 逃过存放msg类型的地址空间 */
        conned_obj_arr[conned_sock]->recv_buf.base_ptr += sizeof(uint32_t);
        /* 有效数据长度减去msg类型的地址空间长度 */
        conned_obj_arr[conned_sock]->recv_buf.e_offset -= sizeof(uint32_t);
        /* 取逻辑包有效数据到链接者的msg容器 */
        memcpy(conned_obj_arr[conned_sock]->msg, conned_obj_arr[conned_sock]->recv_buf.base_ptr,
                conned_obj_arr[conned_sock]->msg_len);
        /* 移动被处理的数据的基地址到下一个将被处理数据的起始地址处 */
        conned_obj_arr[conned_sock]->recv_buf.base_ptr += conned_obj_arr[conned_sock]->msg_len;
        /* 有效数据长度减去已处理消息的长度 */
        conned_obj_arr[conned_sock]->recv_buf.e_offset -= conned_obj_arr[conned_sock]->msg_len;
        /* 处理取到的msg； */
        // handle_package_with_msg(conned_sock);
    }
}

void handle_received_data_bit_stream(const int conned_sock) {
    /* 本次获取数据的长度*/
    conned_obj_arr[conned_sock]->msg_len = conned_obj_arr[conned_sock]->recv_buf.e_offset;
    /* 取有效数据到链接者的msg容器 */
    memcpy(conned_obj_arr[conned_sock]->msg, conned_obj_arr[conned_sock]->recv_buf.base_ptr,
            conned_obj_arr[conned_sock]->msg_len);
    /* 移动被处理的数据的基地址到下一个将被处理数据的起始地址处 */
    conned_obj_arr[conned_sock]->recv_buf.base_ptr += conned_obj_arr[conned_sock]->msg_len;
    /* 有效数据长度减去已处理数据的长度 */
    conned_obj_arr[conned_sock]->recv_buf.e_offset -= conned_obj_arr[conned_sock]->msg_len;
    /* 处理取到的数据； */
    handle_package_with_data(conned_sock);
}

void handle_package_with_data(const int conned_sock) {
    fwrite(conned_obj_arr[conned_sock]->msg, sizeof(char), conned_obj_arr[conned_sock]->msg_len, stdout);
}
