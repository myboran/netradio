#include <stdio.h>
#include <stdlib.h>

#include "medialib.h"
#include "mytbf.h"

struct channel_context_st
{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;
    int pos;
    int fd;
    off_t offset;
    mytbf_t *tbf;
};
