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
#include "tsf_serial_protocol.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

/* the get mode functions are here for historical reasons.  right now all of
 * the actual detection work happens in the adaptive reader. */

tsf_zip_mode_t tsf_stream_file_get_mode_from_fd(int fd) {
    uint8_t magic[TSF_SP_MAGIC_LEN];
    uint8_t *cur=magic;
    
    while (cur<magic+TSF_SP_MAGIC_LEN) {
        int res=read(fd,cur,magic+TSF_SP_MAGIC_LEN-cur);
        if (res<0) {
            tsf_set_errno("Error reading magic code");
            return TSF_ZIP_UNKNOWN;
        }
        if (res==0) {
	    if (cur==magic) {
		tsf_set_error(TSF_E_EOF,
			      "End of file before magic code (file empty or connection closed prior to the transmission of any TSF data)");
	    } else {
		tsf_set_error(TSF_E_UNEXPECTED_EOF,
			      "End of file before end of magic code");
	    }
            return TSF_ZIP_UNKNOWN;
        }
        cur+=res;
    }
    
    if (!memcmp(magic,TSF_SP_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
        return TSF_ZIP_NONE;
    } else if (!memcmp(magic,TSF_SP_ZLIB_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
        return TSF_ZIP_ZLIB;
    } else if (!memcmp(magic,TSF_SP_BZIP2_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
        return TSF_ZIP_BZIP2;
    }
    
    tsf_set_error(TSF_E_PARSE_ERROR,
                  "Not a valid magic code for any TSF stream file format");
    return TSF_ZIP_UNKNOWN;
}

tsf_zip_mode_t tsf_stream_file_get_mode(const char *filename) {
    int fd=open(filename,O_RDONLY);
    tsf_zip_mode_t result;
    
    if (fd<0) {
        tsf_set_errno("Could not open %s for reading",
                      filename);
        return TSF_ZIP_UNKNOWN;
    }
    
    result=tsf_stream_file_get_mode_from_fd(fd);
    
    close(fd);
    
    return result;
}

tsf_stream_file_input_t *tsf_stream_file_input_open(const char *filename,
                                                    tsf_limits_t *limits,
                                                    tsf_zip_rdr_attr_t *attr) {
    int fd;
    tsf_stream_file_input_t *result;
    
    fd=open(filename,O_RDONLY);
    if (fd<0) {
        tsf_set_errno("Could not open %s for reading",
                      filename);
        return NULL;
    }
    
    result=tsf_stream_file_input_fd_open(fd,
                                         limits,
                                         attr,
                                         tsf_true,
                                         0);
    if (result==NULL) {
        close(fd);
        return NULL;
    }
    
    return result;
}

tsf_stream_file_input_t *tsf_stream_file_input_fd_open(int fd,
                                                       tsf_limits_t *limits,
                                                       tsf_zip_rdr_attr_t *_attr,
                                                       tsf_bool_t keep,
                                                       uint32_t buf_size) {
    tsf_stream_file_input_t *ret;
    tsf_zip_rdr_attr_t attr;
    tsf_reader_t reader;
    
    if (_attr==NULL) {
	tsf_zip_rdr_attr_get_default(&attr);
    } else {
	attr=*_attr;
    }
    
    if (buf_size==0) {
        buf_size=TSF_DEF_BUF_SIZE;
    }

    ret=malloc(sizeof(tsf_stream_file_input_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_stream_file_input_t");
        return NULL;
    }
    
    ret->fd=fd;
    ret->keep_fd=keep;
    
    if (tsf_zip_rdr_attr_is_nozip(&attr)) {
        ret->use_adpt=tsf_false;
        ret->reader.buf=tsf_buf_reader_create(ret->fd,
                                              buf_size,
                                              tsf_false);
        reader=tsf_buf_reader_read;
    } else {
	ret->use_adpt=tsf_true;
	ret->reader.adpt=tsf_adaptive_reader_create(tsf_fd_partial_reader,
						    &(ret->fd),
						    buf_size,
						    &attr);
	reader=tsf_adaptive_reader_read;
    }

    if (ret->reader.ptr==NULL) {
        free(ret);
        return NULL;
    }
    
    ret->in_man=tsf_serial_in_man_create(reader,
                                         ret->reader.ptr,
                                         limits);
    if (ret->in_man==NULL) {
        if (ret->use_adpt) {
            tsf_adaptive_reader_destroy(ret->reader.adpt);
        } else {
            tsf_buf_reader_destroy(ret->reader.buf);
        }
        free(ret);
        return NULL;
    }
    
    /* if we ever have a
     * tsf_serial_in_man_pretend_that_youve_read_reset_types() 
     * function, we should call it here. */
    
    return ret;
}

void tsf_stream_file_input_close(tsf_stream_file_input_t *file) {
    tsf_serial_in_man_destroy(file->in_man);
    if (file->keep_fd) {
        close(file->fd);
    } else {
        /* try to seek the file descriptor back to where it should have been,
           if we hadn't been reading with buffering. */
        
        uint32_t num_remaining;
        
        if (file->use_adpt) {
            num_remaining=tsf_adaptive_reader_get_remaining(file->reader.adpt);
        } else {
            num_remaining=tsf_buf_reader_get_remaining(file->reader.buf);
        }
        
        lseek(file->fd,-((off_t)num_remaining),SEEK_CUR);
        
        /* ignore error, since this is best effort anyway. */
    }
    if (file->use_adpt) {
        tsf_adaptive_reader_destroy(file->reader.adpt);
    } else {
        tsf_buf_reader_destroy(file->reader.buf);
    }
    free(file);
}

tsf_bool_t tsf_stream_file_input_allows_compression(tsf_stream_file_input_t *file) {
    return file->use_adpt;
}

tsf_bool_t tsf_stream_file_input_register_type_cback(tsf_stream_file_input_t *file,
                                                     tsf_type_cback_t cback,
                                                     void *arg) {
    return tsf_serial_in_man_register_type_cback(file->in_man, cback, arg);
}

tsf_bool_t tsf_stream_file_input_unregister_type_cback(tsf_stream_file_input_t *file,
                                                       tsf_type_cback_t cback,
                                                       void *arg) {
    return tsf_serial_in_man_unregister_type_cback(file->in_man, cback, arg);
}

tsf_buffer_t *tsf_stream_file_input_read(tsf_stream_file_input_t *file) {
    if (file->use_adpt) {
	tsf_adaptive_reader_hint_switch(file->reader.adpt);
    }
    return tsf_serial_in_man_read(file->in_man);
}

tsf_bool_t tsf_stream_file_input_read_existing_buffer(tsf_stream_file_input_t *file,
                                                      tsf_buffer_t *buf) {
    if (file->use_adpt) {
	tsf_adaptive_reader_hint_switch(file->reader.adpt);
    }
    return tsf_serial_in_man_read_existing_buffer(file->in_man, buf);
}

tsf_bool_t tsf_stream_file_input_get_mark(tsf_stream_file_input_t *file,
                                          tsf_stream_file_mark_t *mark) {
    if (file->use_adpt) {
        tsf_set_error(TSF_E_NOT_SUPPORTED,
                      "tsf_stream_file_input_get_mark() is not supported if "
		      "compression is allowed");
        return tsf_false;
    }

    mark->offset = lseek(file->fd,
                         (off_t) 0,
                         SEEK_CUR);
    if (mark->offset<0) {
        tsf_set_errno("Could not lseek() in "
                      "tsf_stream_file_input_get_mark_of_next()");
        return tsf_false;
    }
    
    mark->offset -= tsf_buf_reader_get_remaining(file->reader.buf);
    
    mark->state = tsf_serial_in_man_get_state(file->in_man);
    
    return tsf_true;
}

tsf_bool_t tsf_stream_file_input_seek(tsf_stream_file_input_t *file,
                                      const tsf_stream_file_mark_t *mark) {
    /* if the file is compressed then the user should not have been able
     * to get a mark. */
    tsf_assert(!file->use_adpt);

    if (lseek(file->fd,
              mark->offset,
              SEEK_SET) < 0) {
        tsf_set_errno("Could not lseek() in "
                      "tsf_stream_file_input_seek_to_mark()");
        return tsf_false;
    }
    
    tsf_buf_reader_reset(file->reader.buf);
    
    tsf_serial_in_man_set_state(file->in_man,
                                mark->state);
    
    return tsf_true;
}




