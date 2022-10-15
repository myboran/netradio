#ifndef THR_CHNNEL_H__
#define THR_CHNNEL_H__

#include "medialib.h"

int thr_channel_create(struct mlib_listentry_st*);
int thr_channel_destory(struct mlib_listentry_st*);
int thr_channel_destoryall(void);

#endif