/*
 *  glue for uvc and venc
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "uvc_venc_glue.h"

void clear_ok_queue()
{
    frame_node_t *node = 0;
    uvc_cache_t *uvc_cache = uvc_cache_get();

    while (0 == get_node_from_queue(uvc_cache->ok_queue, &node))
    {
        node->used = 0;
        put_node_to_queue(uvc_cache->free_queue, node);
    }
}

void add_multi_payload_header(frame_node_t *node, unsigned long stream_type)
{
    unsigned char *mem = node->mem;

    memset(mem, 0, 0x1A);

    //all bytes need to be filled in little endian
    //version
    mem[0] = 0x00;
    mem[1] = 0x01;

    //header length
    mem[2] = 0x16;
    mem[3] = 0x00;

    //stream type
    mem[4] = stream_type & 0xff;
    mem[5] = ((stream_type & 0xff00) >> 8);
    mem[6] = ((stream_type & 0xff0000) >> 16);
    mem[7] = ((stream_type & 0xff000000) >> 24);

    node->used += 0x1A;
}

void set_multi_payload_size(frame_node_t *node)
{
    unsigned long payload_size = node->used - 0x1A;
    unsigned char *payload_mem = node->mem + 0x16;

    // filled value in little endian
    payload_mem[0] = payload_size & 0xff;
    payload_mem[1] = ((payload_size & 0xff00) >> 8);
    payload_mem[2] = ((payload_size & 0xff0000) >> 16);
    payload_mem[3] = ((payload_size & 0xff000000) >> 24);
}
