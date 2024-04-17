/*
 * Copyright (C) 2003, 2004, 2005, 2015 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "tsf_internal.h"
#include "tsf_format.h"

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

tsf_buf_wtr_t *tsf_buf_writer_create(int fd,
                                     uint32_t buf_size,
                                     tsf_bool_t keep_fd) {
    tsf_buf_wtr_t *result;
    
    result=malloc(sizeof(tsf_buf_wtr_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_buf_wtr_t");
        return NULL;
    }
    
    result->buf=malloc(buf_size);
    if (result->buf==NULL) {
        free(result);
        tsf_set_errno("Could not malloc buffer");
        return NULL;
    }

    result->fd=fd;
    result->keep_fd=keep_fd;
    result->size=buf_size;
    result->pos=0;
    
    return result;
}

void tsf_buf_writer_destroy(tsf_buf_wtr_t *buf) {
    /* uh, close the fd if keep_fd is set? */
    free(buf->buf);
    free(buf);
}

tsf_bool_t tsf_buf_writer_write(void *_buf,
                                const void *data,
                                uint32_t len) {
    tsf_buf_wtr_t *buf  =_buf;
    
#ifdef HAVE_WRITEV
    if (buf->pos + len > buf->size) {
        if (buf->pos) {
            struct iovec iov[2];
            iov[0].iov_base = (void*)buf->buf;
            iov[0].iov_len = buf->pos;
            iov[1].iov_base = (void*)data;
            iov[1].iov_len = len;
            for (;;) {
                int res = writev(buf->fd, iov, 2);
                if (res < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    tsf_set_errno("Calling writev() in "
                                  "tsf_buf_writer_write()");
                    return tsf_false;
                }
                if ((unsigned)res >= iov[0].iov_len) {
                    data = ((uint8_t*)data) + res-iov[0].iov_len;
                    len -= res - iov[0].iov_len;
                    break;
                }
                iov[0].iov_base += res;
                iov[0].iov_len -= res;
            }
            buf->pos = 0;
            if (len == 0) {
                return tsf_true;
            }
        }
        tsf_assert(buf->pos == 0);
        if (len > buf->size) {
            return tsf_fd_writer(&buf->fd, data, len);
        }
    }
#else
    if (buf->pos + len >= buf->size) {
        /* have to flush because there isn't enough room in our buffer
         * otherwise. */
        if (!tsf_buf_writer_flush(buf)) {
            return tsf_false;
        }
    }
    
    if (len >= buf->size && buf->pos == 0) {
        /* we just write now because we are being given more data than
         * our buffer could store. */
        return tsf_fd_writer(&buf->fd, data, len);
    }
#endif
    
    /*
    printf(" buf->buf = %p\n",buf->buf);
    printf(" buf->pos = " fui32 "\n",buf->pos);
    printf("buf->size = " fui32 "\n",buf->size);
    printf("     data = %p\n",data);
    printf("      len = " fui32 "\n",len);
    */
    
    memcpy(buf->buf + buf->pos, data, len);
    buf->pos += len;
    
    return tsf_true;
}

tsf_bool_t tsf_buf_writer_flush(tsf_buf_wtr_t *buf) {
    if (!tsf_fd_writer(&buf->fd,buf->buf,buf->pos)) {
        return tsf_false;
    }
    buf->pos=0;
    return tsf_true;
}



