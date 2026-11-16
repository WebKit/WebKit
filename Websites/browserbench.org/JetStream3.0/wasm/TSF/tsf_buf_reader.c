/*
 * Copyright (C) 2003, 2004, 2005 Filip Pizlo. All rights reserved.
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

tsf_buf_rdr_t *tsf_buf_reader_create(int fd,
                                     uint32_t buf_size,
                                     tsf_bool_t keep_fd) {
    tsf_buf_rdr_t *result;
    
    result=malloc(sizeof(tsf_buf_rdr_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_buf_rdr_t");
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
    result->puti=0;
    result->geti=0;
    
    return result;
}

void tsf_buf_reader_destroy(tsf_buf_rdr_t *buf) {
    /* uh, close the fd if keep_fd is set? */
    free(buf->buf);
    free(buf);
}

tsf_bool_t tsf_buf_reader_read(void *_buf,
                               void *data,
                               uint32_t len) {
    tsf_bool_t read_some=tsf_false;
#if defined(HAVE_READV)
    struct iovec iov[2];
#endif
    tsf_buf_rdr_t *buf=_buf;
    
    /* get as much as possible from the buffer */
    if (buf->puti>buf->geti) {
        uint32_t avail=buf->puti-buf->geti;
        
        read_some=tsf_true;
        
        if (avail>len) {
            /* can be satisfied entirely from the buffer */
            memcpy(data,buf->buf+buf->geti,len);
            buf->geti+=len;
            return tsf_true;
        }
        
        memcpy(data,buf->buf+buf->geti,avail);
        
        data=((uint8_t*)data)+avail;
        len-=avail;
    }

    buf->puti=0;
    buf->geti=0;
    
    if (!len) {
        return tsf_true;
    }
    
    /* now satisfy the rest of the request by issuing a new read */
#if defined(HAVE_READV)
    iov[0].iov_base=data;
    iov[0].iov_len=len;
    iov[1].iov_base=(void*)buf->buf;
    iov[1].iov_len=buf->size;
    
    for (;;) {
        int res=readv(buf->fd,iov,2);
        if (res<0) {
            if (errno==EINTR) {
                continue;
            }
            tsf_set_errno("Calling readv() in tsf_buf_reader_read()");
            return tsf_false;
        }
        if (res==0) {
            tsf_set_error(read_some?
                          TSF_E_ERRONEOUS_EOF:TSF_E_UNEXPECTED_EOF,
                          "Calling readv() in tsf_buf_reader_read()");
            return tsf_false;
        }
        read_some=tsf_true;
        if ((unsigned)res>=iov[0].iov_len) {
            buf->puti=res-iov[0].iov_len;
            break;
        }
        iov[0].iov_base+=res;
        iov[0].iov_len-=res;
    }
#else
    if (len>=buf->size) {
        tsf_bool_t res=tsf_fd_reader(&buf->fd,data,len);
        if (read_some && !res &&
            tsf_get_error_code()==TSF_E_UNEXPECTED_EOF) {
            tsf_set_error(TSF_E_ERRONEOUS_EOF,
                          "tsf_fd_reader() experienced an EOF");
        }
        return res;
    }
    
    /* idea is this: we keep reading until we have enough to satisfy the
     * current request, but we allow the read to overflow for as many bytes
     * as we have available in the buffer. */
    while (buf->puti<len) {
        int res=read(buf->fd,
                     buf->buf+buf->puti,
                     buf->size-buf->puti);
        if (res<0) {
            if (errno==EINTR) {
                continue;
            }
            tsf_set_errno("Calling read() in tsf_buf_reader_read()");
            return tsf_false;
        }
        if (res==0) {
            tsf_set_error(read_some?
                          TSF_E_ERRONEOUS_EOF:TSF_E_UNEXPECTED_EOF,
                          "Calling read() in tsf_buf_reader_read()");
            return tsf_false;
        }
        read_some=tsf_true;
        buf->puti+=res;
    }
    
    memcpy(data,buf->buf,len);
    buf->geti=len;
#endif
    
    return tsf_true;
}

uint32_t tsf_buf_reader_get_remaining(tsf_buf_rdr_t *buf) {
    return buf->puti - buf->geti;
}

void tsf_buf_reader_reset(tsf_buf_rdr_t *buf) {
    buf->puti=0;
    buf->geti=0;
}

