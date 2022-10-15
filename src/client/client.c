#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>
#include <unistd.h>

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

int main(int argc, char *argv[])
{
    pid_t pid;
    int pd[2];
    struct sockaddr_in laddr;
    int c, sd, s, val;
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
    mreq.imr_ifindex = if_nametoindex("ens33");
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

    pid = fork();
    if (pid < 0)
    {
        perror("fork()");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        // 子进程：调用解码器
    }
    // 父进程：从网络上收包，发送给子进程
    // 收节目单
    // 选择频道
    // 收频道包，并且发送给子进程
    exit(EXIT_SUCCESS);
}