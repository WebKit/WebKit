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

char *tsf_read_string(tsf_reader_t reader,
                      void *arg,
                      size_t max_size) {
    uint8_t ui8_tmp;

    const size_t step=128;
    size_t size=step;
    
    uint32_t i=0;
    
    char *str=malloc(size);
    
    for (;;) {
        uint32_t n;
        tsf_bool_t done=tsf_false;
        
        if (str==NULL) {
            tsf_set_errno("Could not (m|re)alloc string");
            return NULL;
        }
        
        for (n=step;n-->0;) {
            if (!reader(arg,&ui8_tmp,1)) {
                return NULL;
            }
            str[i++]=ui8_tmp;
            if (ui8_tmp==0) {
                done=tsf_true;
                break;
            }
        }
        
        if (done) {
            break;
        }
        
        size+=step;
        if (size>max_size) {
            tsf_set_error(TSF_E_PARSE_ERROR,
                          "Remote end attempted to send a string that "
                          "exceeded our limit of " fsz ".",max_size);
            free(str);
            return NULL;
        }
        str=realloc(str,size);
    }
    
    return str;
}

tsf_bool_t tsf_write_string(tsf_writer_t writer,
                            void *arg,
                            const char *str) {
    const char *cur_char=str;

    do {
        if (!writer(arg,(void*)cur_char,1)) {
            return tsf_false;
        }
    } while (*cur_char++);
    
    return tsf_true;
}

tsf_bool_t tsf_full_read_of_partial(tsf_partial_reader_t reader,
				    void *arg,
				    void *_data,
				    uint32_t len) {
    uint8_t *data=_data;
    while (len>0) {
	int32_t res=reader(arg,data,len);
	if (res<0) {
	    if (tsf_get_error_code()==TSF_E_UNEXPECTED_EOF
		&& data!=_data) {
                char *previous_error = strdup(tsf_get_error());
		tsf_set_error(TSF_E_ERRONEOUS_EOF,
			      "End-of-file after part of a full read; original "
			      "error: %s", previous_error);
                free(previous_error);
	    }
	    return tsf_false;
	}
	data+=res;
	len-=res;
    }
    return tsf_true;
}

