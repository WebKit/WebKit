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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

tsf_bool_t tsf_fd_writer(void *arg, const void *data, uint32_t len) {
    uint8_t *cur = (uint8_t*)data;
    int fd = *((int*)arg);
    while (len) {
        int res = write(fd, cur, len);
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }
            tsf_set_errno("Calling write() in tsf_fd_writer()");
            return tsf_false;
        }
        cur += res;
        len -= res;
    }
    return tsf_true;
}

tsf_bool_t tsf_fd_reader(void *arg, void *data, uint32_t len) {
    uint8_t *cur=data;
    int fd=*((int*)arg);
    while (len) {
        int res=read(fd,cur,len);
        if (res<0) {
            if (errno==EINTR) {
                continue;
            }
            tsf_set_errno("Calling read() in tsf_fd_reader()");
            return tsf_false;
        }
        if (res==0) {
            tsf_set_error((void*)cur==data?
                          TSF_E_UNEXPECTED_EOF:TSF_E_ERRONEOUS_EOF,
                          "Calling read() in tsf_fd_reader()");
            return tsf_false;
        }
        cur+=res;
        len-=res;
    }
    return tsf_true;
}

int32_t tsf_fd_partial_reader(void *arg,
                              void *data,
                              uint32_t len) {
    int fd=*((int*)arg);
    for (;;) {
        int res=read(fd,data,len);
        if (res<0) {
            if (errno==EINTR) {
                continue;
            }
            tsf_set_errno("Calling read() in tsf_fd_partial_reader()");
            return -1;
        }
        if (res==0) {
            tsf_set_error(TSF_E_UNEXPECTED_EOF,
                          "Calling read() in tsf_fd_partial_reader()");
            return -1;
        }
        return res;
    }
}

tsf_bool_t tsf_comparing_writer(void *arg,
				const void *buf,
				uint32_t len) {
    tsf_comparing_writer_t *self=arg;
    void *my_buf=malloc(len);
    if (my_buf==NULL) {
	tsf_set_errno("Could not allocate temporary buffer");
	return tsf_false;
    }
    if (self->reader(self->arg,my_buf,len)) {
	int res=memcmp(buf,my_buf,len);
	free(my_buf);
	if (res==0) {
	    return tsf_true;
	} else {
	    tsf_set_error(TSF_E_COMPARE_FAIL,NULL);
	    return tsf_false;
	}
    } else {
	free(my_buf);
	return tsf_false;
    }
}

tsf_bool_t tsf_hashing_writer(void *arg,
			      const void *_buf,
			      uint32_t len) {
    uint8_t *buf = (uint8_t*)_buf;
    uint32_t *hash = arg;
    uint32_t h = *hash, g;
    
    /* this is the ELF hash, at least according to my hashtable
       library. */
    while (len --> 0) {
	h = (h << 4) + buf[len];
	g = h & 0xf0000000;
	if (g) {
	    h ^= g >> 24;
	}
	h &= ~g;
    }

    *hash = h;

    return tsf_true;
}

tsf_bool_t tsf_size_calc_writer(void *arg,
                                const void *buf,
                                uint32_t len) {
    uint32_t *total_len=arg;
    (*total_len)+=len;
    return tsf_true;
}

tsf_bool_t tsf_size_aware_reader(void *arg,
                                 void *buf,
                                 uint32_t len) {
    tsf_size_aware_rdr_t *self=arg;
    
    if (self->limit<len) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Overran size limit for %s",self->name);
        return tsf_false;
    }
    
    if (!self->reader(self->arg,buf,len)) {
        return tsf_false;
    }
    
    self->limit-=len;
    
    return tsf_true;
}

tsf_bool_t tsf_buffer_only_writer(void *arg,
                                  const void *buf,
                                  uint32_t len) {
    uint8_t **cur = arg;
    memcpy(*cur, buf, len);
    (*cur) += len;
    return tsf_true;
}

tsf_bool_t tsf_resizable_buffer_writer(void *arg,
                                       const void *buf,
                                       uint32_t len) {
    tsf_resizable_buffer_t *resizable = arg;
    
    if (resizable->end - resizable->cur < len) {
        size_t old_size;
        size_t new_size;
        uint8_t *new_base;
        
        old_size = resizable->end - resizable->base;
        new_size = (old_size + len) << 1;
        
        if (resizable->base == NULL) {
            new_base = malloc(new_size);
        } else {
            new_base = realloc(resizable->base, new_size);
        }
        
        if (new_base == NULL) {
            tsf_set_errno("Could not malloc resizable buffer");
            return tsf_false;
        }
        
        resizable->end = new_base + new_size;
        resizable->cur = new_base + (resizable->cur - resizable->base);
        resizable->base = new_base;
    }
    
    memcpy(resizable->cur, buf, len);
    resizable->cur += len;
    return tsf_true;
}

tsf_bool_t tsf_buffer_only_reader(void *arg,
                                  void *buf,
                                  uint32_t len) {
    tsf_light_buffer_t *state=arg;
    if (state->cur == state->end) {
        tsf_set_error(TSF_E_UNEXPECTED_EOF,
                      "Buffer is empty");
        return tsf_false;
    }
    if (state->cur+len > state->end) {
        tsf_set_error(TSF_E_ERRONEOUS_EOF,
                      "Buffer does not have requested data");
        return tsf_false;
    }
    memcpy(buf,state->cur,len);
    state->cur+=len;
    return tsf_true;
}


