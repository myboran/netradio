#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <errno.h>

#include "client.h"
#include <proto.h>

/*
-M --mgroup  指定多播组
-P --port    制定接受端口
-p --player  制定播放器
-H --help    显示帮助
*/
struct client_conf_st client_conf = {
    .rcvport = DEFAULT_RCVPORT,
    .mgroup = DEFAULT_MGROUP,
    .player_cmd = DEFAULT_PLAYERCMD};

static void printhelp(void)
{
    printf("-P --port  制定接受端口\n-M --mgroup    指定多播组\n-p --player  制定播放器\n-H --help    显示帮助\n");
}

static int writen(int fd, const char *buf, size_t len)
{
    int pos = 0;
    int ret;
    while (len > 0)
    {
        ret = write(fd, buf + pos, len);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            perror("write()");
            return -1;
        }
        len -= ret;
        pos += ret;
    }
    return pos;
}

int main(int argc, char *argv[])
{
    int chosenid;
    ssize_t len;
    pid_t pid;
    int pd[2];
    struct sockaddr_in laddr, serveraddr, raddr;
    socklen_t serveraddr_len, raddr_len;
    int c, sd, s, val, ret;
    int index = 0;
    struct ip_mreqn mreq;
    struct option argarr[] = {{"port", 1, NULL, 'P'}, {"mgroup", 1, NULL, 'M'}, {"player", 1, NULL, 'p'}, {"help", 1, NULL, 'H'}, {NULL, 0, NULL, 0}};
    /*
        初始化
        级别：默认值，配置文件，环境变量，命令行参数
    */
    while (1)
    {
        c = getopt_long(argc, argv, "P:M:p:H", argarr, &index);
        if (c < 0)
            break;
        switch (c)
        {
        case 'P':
            client_conf.rcvport = optarg;
            break;
        case 'M':
            client_conf.mgroup = optarg;
            break;
        case 'p':
            client_conf.player_cmd = optarg;
            break;
        case 'H':
            printhelp();
            exit(EXIT_SUCCESS);
            break;
        default:
            abort();
            break;
        }
    }

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    s = inet_pton(AF_INET, client_conf.mgroup, &mreq.imr_multiaddr);
    if (s <= 0)
    {
        if (s == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    s = inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
    if (s <= 0)
    {
        if (s == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    mreq.imr_ifindex = if_nametoindex("eth0");
    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))");
        exit(EXIT_FAILURE);
    }
    val = 1;
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0)
    {
        perror("setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val))");
        exit(EXIT_FAILURE);
    }

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(client_conf.rcvport));
    s = inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr.s_addr);
    if (s <= 0)
    {
        if (s == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if (bind(sd, (void *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(EXIT_FAILURE);
    }
    if (pipe(pd) < 0)
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }
    puts("before fork()");
    pid = fork();
    if (pid < 0)
    {
        perror("fork()");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        // 子进程：调用解码器
        close(sd);
        close(pd[1]);
        dup2(pd[0], 0);
        if (pd[0] > 0)
            close(pd[0]);

        execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
        perror("execl()");
        exit(EXIT_FAILURE);
    }
    // 父进程：从网络上收包，发送给子进程
    // 收节目单
    struct msg_list_st *msg_list;
    msg_list = malloc(MSG_LIST_MAX);
    if (msg_list == NULL)
    {
        perror("malloc(MSG_LIST_MAX)");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0, (void *)&serveraddr, &serveraddr_len);
        if (len < sizeof(struct msg_list_st))
        {
            fprintf(stderr, "message is too small.\n");
            continue;
        }
        if (msg_list->chnid != LISTCHNID)
        {
            fprintf(stderr, "chnid is not match.\n");
            continue;
        }
        break;
    }
    // 打印节目单并选择频道
    struct msg_listentry_st *pos;
    for (pos = msg_list->entry; (char *)pos < (((char *)msg_list) + len); pos = (void *)(((char *)pos) + ntohs(pos->len)))
    {
        printf("channel %d: %s\n", pos->chnid, pos->desc);
    }

    free(msg_list);

    while (1)
    {
        ret = scanf("%d", &chosenid);
        if (ret != 1)
            exit(EXIT_FAILURE);
        if (ret == 1)
            break;
    }

    // 收频道包，并且发送给子进程
    struct msg_channel_st *msg_channel;
    msg_channel = malloc(MSG_CHANNEL_MAX);
    if (msg_channel == NULL)
    {
        perror("malloc(MSG_CHANNEL_MAX)()");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        len = recvfrom(sd, msg_channel, MSG_CHANNEL_MAX, 0, (void *)&raddr, &raddr_len);
        if (raddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr || raddr.sin_port != serveraddr.sin_port)
        {
            fprintf(stderr, "Ignore: address not math.\n");
            continue;
        }
        if (len < sizeof(struct msg_channel_st))
        {
            fprintf(stderr, "Ignore: message too small.\n");
            continue;
        }
        if (msg_channel->chnid == chosenid)
        {
            fprintf(stdout, "accepted msg:%d recieved.\n", msg_channel->chnid);

            if (writen(pd[1], msg_channel->data, len - sizeof(chnid_t)) < 0)
            {
                fprintf(stderr, "writen err.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    free(msg_channel);
    close(sd);

    exit(EXIT_SUCCESS);
}