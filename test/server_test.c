#include "../utils.h"
#include "../net_utils.h"

/* gcc ../*.h ../*c server_test.c -o server_test -l curl */
int main(int argc, const char *argv[])
{
    int l_sock, epollfd;
    epollfd = init_epoll();
    l_sock = init_listen_sock(1111, "0.0.0.0", SOCK_STREAM, 10);
    // l_sock = init_listen_sock(1111, "127.0.0.1", SOCK_STREAM, 10);
    // add_listen_sock2epoll(epollfd, l_sock);
    // l_sock = init_listen_sock(1111, "192.168.100.59", SOCK_STREAM, 10);
    add_listen_sock2epoll(epollfd, l_sock);
    handle_sock_from_epoll(epollfd,
            handle_first_connected,
            handle_after_first_connected);
    printf("first\n");
    return 0;
}
