#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "../include/proto.h"
#include "thr_list.h"
#include "server_conf.h"
#include "medialib.h"

static pthread_t tid_list;
static int nr_list_ent;
static struct mlib_listentry_st *list_ent;

static void *thr_list(void *p)
{
    int totalsize;
    struct msg_list_st *entlistp;
    struct msg_listentry_st *entryp;
    int ret;
    int size;
    totalsize = sizeof(chnid_t);
    for (int i = 0; i < nr_list_ent; i++)
    {
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
        // totalsize += sizeof(entryp) + strlen(list_ent[i].desc);
    }
    entlistp = malloc(totalsize);
    if (entlistp == NULL)
    {
        syslog(LOG_ERR, "malloc():%s", strerror(errno));
        exit(1);
    }
    entlistp->chnid = LISTCHNID;
    entryp = entlistp->entry;
    syslog(LOG_DEBUG, "nr_list_entn:%d\n", nr_list_ent);
    for (int i = 0; i < nr_list_ent; i++)
    {
        size = sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);

        entryp->chnid = list_ent[i].chnid;
        entryp->len = htons(size);
        strcpy(entryp->desc, list_ent[i].desc);
        entryp = (void *)(((char *)entryp) + size);
        syslog(LOG_DEBUG, "entryp len:%d\n", entryp->len);
    }

    while (1)
    {
        ret = sendto(serversd, entlistp, totalsize, 0, (void *)&sndaddr, sizeof(sndaddr));
        syslog(LOG_DEBUG, "sent content len:%d\n", entlistp->entry->len);
        if (ret < 0)
        {
            syslog(LOG_WARNING, "sendto(serversd, enlistp...:%s", strerror(errno));
        }
        else
        {
            syslog(LOG_DEBUG, "sendto(serversd, enlistp....):success");
        }
        sleep(1);
    }
}

int thr_list_create(struct mlib_listentry_st *listp, int nr_ent)
{
    int err;
    list_ent = listp;
    nr_list_ent = nr_ent;
    syslog(LOG_DEBUG, "list content: chnid:%d, desc:%s\n", listp->chnid, listp->desc);
    err = pthread_create(&tid_list, NULL, thr_list, NULL);
    if (err)
    {
        syslog(LOG_ERR, "pthread_create():%s", strerror(errno));
        return -1;
    }
    return 0;
}

int thr_list_destory(void)
{
    pthread_cancel(tid_list);
    pthread_join(tid_list, NULL);
    return 0;
}