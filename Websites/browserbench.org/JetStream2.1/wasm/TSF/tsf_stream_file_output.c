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
#include "tsf_serial_protocol.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

tsf_stream_file_output_t *tsf_stream_file_output_open(const char *filename,
                                                      tsf_bool_t append,
                                                      tsf_zip_wtr_attr_t *attr) {
    int fd;
    tsf_stream_file_output_t *result;
    int flags=O_WRONLY|(append?O_APPEND:O_TRUNC);
    tsf_bool_t created=tsf_false;
    tsf_zip_wtr_attr_t defaults;

    if (attr==NULL) {
        attr=&defaults;
        tsf_zip_wtr_attr_get_default(attr);
    }
    
    /* this is written in a totally crazy frikkin' way so that it cleans
     * up after itself correctly in case of error. */
    
    for (;;) {
        /* first try to open an existing file */
        fd=open(filename,flags);
        
        if (fd>=0) {
            break;
        }
        
        if (errno!=ENOENT) {
            tsf_set_errno("Could not open %s for writing",
                          filename);
            return NULL;
        }
        
        /* the file didn't exist, so try creating it. */
        fd=open(filename,O_CREAT|O_EXCL|flags,0666);
        
        if (fd>=0) {
            created=tsf_true;
            break;
        }
        
        if (errno!=EEXIST) {
            tsf_set_errno("Could not open %s for writing",
                          filename);
            return NULL;
        }
    }
    
    result=tsf_stream_file_output_fd_open(fd,attr,tsf_true,0);
    if (result==NULL) {
        close(fd);
        
        /* this is where the craziness above gets used.  the loop above
         * ensures that we always know if we were the ones who created
         * the file.  if we were the ones who created, we delete it
         * now.  but if we were not the ones who created, we leave it
         * alone. */
	/* FIXME: not sure if this is correct.  even if we didn't create
	   the file, we still end up writing a bunch of garbage into it.
	   so maybe the correct thing to do is to either always delete it,
	   never delete it, or somehow preserve the old contents of the file
	   to be able to restore said contents here. */
        if (created) {
            unlink(filename);
        }
        return NULL;
    }
    
    return result;
}

tsf_stream_file_output_t *tsf_stream_file_output_fd_open(int fd,
                                                         tsf_zip_wtr_attr_t *attr,
                                                         tsf_bool_t keep,
                                                         uint32_t buf_size) {
    tsf_stream_file_output_t *ret;
    tsf_zip_wtr_attr_t defaults;
    tsf_writer_t writer;
    
    if (attr==NULL) {
        attr=&defaults;
        tsf_zip_wtr_attr_get_default(attr);
    }
    
    if (buf_size==0) {
        buf_size=TSF_DEF_BUF_SIZE;
    }
    
    ret=malloc(sizeof(tsf_stream_file_output_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_stream_file_output_t");
        return NULL;
    }
    
    ret->all_good=tsf_true;
    ret->fd=fd;
    ret->keep_fd=keep;
    
    switch (attr->mode) {
        case TSF_ZIP_NONE:
            ret->use_zip=tsf_false;
            ret->writer.buf=tsf_buf_writer_create(ret->fd,
                                                  buf_size,
                                                  tsf_false);
            writer=tsf_buf_writer_write;
            break;
        case TSF_ZIP_ZLIB:
        case TSF_ZIP_BZIP2:
            if (!tsf_fd_writer(&fd,
                               attr->mode==TSF_ZIP_ZLIB?
                               TSF_SP_ZLIB_MAGIC_CODE:
                               TSF_SP_BZIP2_MAGIC_CODE,
                               TSF_SP_MAGIC_LEN)) {
                free(ret);
                return NULL;
            }
    
            ret->use_zip=tsf_true;
            ret->writer.zip=tsf_zip_writer_create(tsf_fd_writer,
                                                  &(ret->fd),
                                                  attr);
            writer=tsf_zip_writer_write;
            break;
        default:
            tsf_set_error(TSF_E_INVALID_ARG,
                          "%d is not a valid zip mode",
                          attr->mode);
            free(ret);
            return NULL;
    }

    if (ret->writer.ptr==NULL) {
        free(ret);
        return NULL;
    }
    
    ret->out_man=tsf_serial_out_man_create(writer,
                                           ret->writer.ptr);
    if (ret->out_man==NULL) {
        if (ret->use_zip) {
            tsf_zip_writer_destroy(ret->writer.zip);
        } else {
            tsf_buf_writer_destroy(ret->writer.buf);
        }
        free(ret);
        return NULL;
    }
    
    return ret;
}

tsf_bool_t tsf_stream_file_output_close(tsf_stream_file_output_t *file) {
    tsf_bool_t result=tsf_true;
    
    if (file->all_good) {
        if (file->use_zip) {
            result=tsf_zip_writer_finish(file->writer.zip);
        } else {
            result=tsf_buf_writer_flush(file->writer.buf);
        }
    }
    
    tsf_serial_out_man_destroy(file->out_man);
    if (file->use_zip) {
        tsf_zip_writer_destroy(file->writer.zip);
    } else {
        tsf_buf_writer_destroy(file->writer.buf);
    }
    if (file->keep_fd) {
        close(file->fd);
    }
    free(file);

    return result;
}

tsf_bool_t tsf_stream_file_output_flush(tsf_stream_file_output_t *file) {
    tsf_bool_t result;
    if (file->use_zip) {
        result=tsf_zip_writer_flush(file->writer.zip,tsf_false);
    } else {
        result=tsf_buf_writer_flush(file->writer.buf);
    }
    if (!result) {
        file->all_good=tsf_false;
    }
    return result;
}

tsf_bool_t tsf_stream_file_output_write(tsf_stream_file_output_t *file,
                                        tsf_buffer_t *buffer) {
    tsf_bool_t result=tsf_serial_out_man_write(file->out_man,buffer);
    if (!result) {
        file->all_good=tsf_false;
    }
    return result;
}



