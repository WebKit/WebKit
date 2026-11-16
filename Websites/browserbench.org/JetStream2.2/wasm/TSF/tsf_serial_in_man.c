/*
 * Copyright (C) 2003, 2004, 2005, 2014, 2015 Filip Pizlo. All rights reserved.
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
#include "tsf_format.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

tsf_serial_in_man_t *tsf_serial_in_man_create(tsf_reader_t reader,
                                              void *arg,
                                              tsf_limits_t *limits) {
    tsf_serial_in_man_t *ret=malloc(sizeof(tsf_serial_in_man_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_serial_in_man_t");
        return NULL;
    }
    
    ret->types=tsf_type_in_map_create();
    if (ret->types==NULL) {
        free(ret);
        return NULL;
    }
    
    if (limits==NULL) {
        ret->limits=NULL;
    } else {
        ret->limits=tsf_limits_clone(limits);
        if (ret->limits==NULL) {
            tsf_type_in_map_destroy(ret->types);
            free(ret);
            return NULL;
        }
    }
    
    ret->state=0;
    
    ret->types_byte_size=0;
    
    ret->cbacks=NULL;
    ret->num_cbacks=0;
    
    ret->reader=reader;
    ret->arg=arg;

    return ret;
}

void tsf_serial_in_man_destroy(tsf_serial_in_man_t *in_man) {
    tsf_type_in_map_destroy(in_man->types);
    if (in_man->cbacks!=NULL) {
        free(in_man->cbacks);
    }
    
    tsf_limits_destroy(in_man->limits);
    
    free(in_man);
}

tsf_bool_t tsf_serial_in_man_read_reset_types(tsf_serial_in_man_t *in_man) {
    tsf_type_in_map_t *new_types;
    char buf[TSF_SP_MAGIC_LEN];
    tsf_bool_t result=tsf_true;
    
    new_types=tsf_type_in_map_create();
    if (new_types==NULL) {
        return tsf_false;
    }
    
    if (!in_man->reader(in_man->arg,buf,TSF_SP_MAGIC_LEN)) {
        result=tsf_false;
        goto done;
    }
    
    if (memcmp(buf,TSF_SP_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "The input stream did not contain a valid reset "
                      "types command");
        result=tsf_false;
        goto done;
    }
    
    tsf_type_in_map_destroy(in_man->types);

    in_man->types=new_types;
    in_man->state=0;
    in_man->types_byte_size=0;

done:
    return result;
}

tsf_bool_t tsf_serial_in_man_read_reset_types_from_fd(int fd) {
    tsf_serial_in_man_t *in_man;
    tsf_bool_t res;
    
    in_man=tsf_serial_in_man_create(tsf_fd_reader,
                                    &fd,
                                    NULL);
    if (in_man==NULL) {
        return tsf_false;
    }
    
    res=tsf_serial_in_man_read_reset_types(in_man);
    
    tsf_serial_in_man_destroy(in_man);
    
    return res;
}

tsf_bool_t tsf_serial_in_man_read_reset_types_from_file(const char *filename) {
    int fd;
    tsf_bool_t res;
    
    fd=open(filename, O_RDONLY);
    if (fd<0) {
        tsf_set_errno("Trying to open %s to check reset types command",
                      filename);
        return tsf_false;
    }
    
    res=tsf_serial_in_man_read_reset_types_from_fd(fd);
    
    close(fd);
    
    return res;
}

tsf_bool_t tsf_serial_in_man_register_type_cback(tsf_serial_in_man_t *in_man,
                                                 tsf_type_cback_t cback,
                                                 void *arg) {
    if (in_man->cbacks==NULL) {
        in_man->cbacks=malloc(sizeof(tsf_serial_in_man_cback_t));
        if (in_man->cbacks==NULL) {
            tsf_set_errno("Could not malloc tsf_serial_in_man_cback_t");
            goto error;
        }
    } else {
        tsf_serial_in_man_cback_t *new_array=
            realloc(in_man->cbacks,
                    sizeof(tsf_serial_in_man_cback_t)*(in_man->num_cbacks+1));
        if (new_array==NULL) {
            tsf_set_errno("Could not realloc array of "
                          "tsf_serial_in_man_cback_t");
            goto error;
        }
        in_man->cbacks=new_array;
    }
    
    in_man->cbacks[in_man->num_cbacks].cback=cback;
    in_man->cbacks[in_man->num_cbacks].arg=arg;
    
    in_man->num_cbacks++;
    
    return tsf_true;

error:
    return tsf_false;
}

uint32_t tsf_serial_in_man_unregister_type_cback(tsf_serial_in_man_t *in_man,
                                                 tsf_type_cback_t cback,
                                                 void *arg) {
    uint32_t i,ret=0;

    for (i=0;i<in_man->num_cbacks;++i) {
        if (in_man->cbacks[i].cback==cback &&
            in_man->cbacks[i].arg==arg) {
            memcpy(in_man->cbacks+--in_man->num_cbacks,
                   in_man->cbacks+i--,
                   sizeof(tsf_serial_in_man_cback_t));
            ret++;
        }
    }

    return ret;
}

tsf_bool_t tsf_serial_in_man_read_existing_buffer(tsf_serial_in_man_t *in_man,
                                                  tsf_buffer_t *result) {
    tsf_type_t *type;
    tsf_bool_t res;
    uint8_t command;
    char buf[256];
    tsf_type_in_map_t *new_types;
    uint32_t n;
    
    for (;;) {
        if (!in_man->reader(in_man->arg, &command, sizeof(command))) {
            /* if the EOF happens here and only here, then the EOF
             * can be said to have been expected. */
            if (tsf_get_error_code() == TSF_E_UNEXPECTED_EOF) {
                tsf_set_error(TSF_E_EOF, NULL);
            }

            return tsf_false;
        }
        
        switch (command) {
        case TSF_SP_C_RESET_TYPES:
            /* first read the magic string */
            if (!in_man->reader(in_man->arg, buf, TSF_SP_MAGIC_SUFFIX_LEN)) {
                return tsf_false;
            }
                
            if (memcmp(buf,
                       TSF_SP_MAGIC_SUFFIX,
                       TSF_SP_MAGIC_SUFFIX_LEN)) {
                tsf_set_error(TSF_E_PARSE_ERROR,
                              "Bad magic header for TSF_SP_C_RESET_TYPES "
                              "command");
                return tsf_false;
            }
                
            new_types = tsf_type_in_map_create();
            if (new_types == NULL) {
                return tsf_false;
            }
            tsf_type_in_map_destroy(in_man->types);
            in_man->types = new_types;
            in_man->state = 0;
            in_man->types_byte_size = 0;
            break;
        case TSF_SP_C_NEW_TYPE:
            if (tsf_type_in_map_get_num_types(in_man->types)
                == tsf_limits_max_types(in_man->limits)) {
                tsf_set_error(TSF_E_PARSE_ERROR,
                              "Too many types in one stream");
                return tsf_false;
            }
                
            type = tsf_type_read(in_man->reader,
                                 in_man->arg,
                                 in_man->limits);
            if (type == NULL) {
                return tsf_false;
            }
                
            if (tsf_type_in_map_get_num_types(in_man->types)
                > in_man->state) {
                /* this should be a type that we've seen already, so
                 * just verify that it is the right one. */
                    
                if (!tsf_type_compare(
                        tsf_type_in_map_get_type(in_man->types,
                                                 in_man->state),
                        type)) {
                    tsf_set_error(TSF_E_UNEXPECTED_TYPE,
                                  "While in a seek-back state with "
                                  "state = " fui32 " and num_types = "
                                  fui32,
                                  in_man->state,
                                  tsf_type_in_map_get_num_types(
                                      in_man->types));
                    tsf_type_destroy(type);
                    return tsf_false;
                }
            } else {
                /* a new type, so append it to the map. */
                    
                if (in_man->types_byte_size +
                    tsf_type_get_byte_size(type)
                    > tsf_limits_max_total_type_size(in_man->limits)) {
                    tsf_set_error(TSF_E_PARSE_ERROR,
                                  "Total size of types in stream too big");
                    tsf_type_destroy(type);
                    return tsf_false;
                }
                    
                in_man->types_byte_size += tsf_type_get_byte_size(type);
                    
                for (n = in_man->num_cbacks; n --> 0;) {
                    in_man->cbacks[n].cback(in_man,
                                            type,
                                            in_man->cbacks[n].arg);
                }
                    
                res = tsf_type_in_map_append(in_man->types, type);
                if (!res) {
                    tsf_type_destroy(type);
                    return tsf_false;
                }
            }
                
            tsf_type_destroy(type);
                
            in_man->state++;
            
            break;
        case TSF_SP_C_DATA:
            return tsf_buffer_read_simple(result,
                                          in_man->types,
                                          in_man->reader,
                                          in_man->arg,
                                          in_man->limits);
        default:
            if (command & TSF_SP_C_CF_DATA_BIT) {
                return tsf_buffer_read_small_with_type_code(result,
                                                            command & ~TSF_SP_C_CF_DATA_BIT,
                                                            in_man->types,
                                                            in_man->reader,
                                                            in_man->arg,
                                                            in_man->limits);
            }
            
            tsf_set_error(TSF_E_PARSE_ERROR,
                          "Unexpected command read from serial stream: %u", (unsigned)command);
            return tsf_false;
        }
    }
    
    tsf_abort("Should never get here");
    return tsf_false;
}

tsf_buffer_t *tsf_serial_in_man_read(tsf_serial_in_man_t *in_man) {
    tsf_buffer_t *result;
    
    result = tsf_buffer_create_bare();
    if (!result) {
        return NULL;
    }
    
    if (!tsf_serial_in_man_read_existing_buffer(in_man, result)) {
        tsf_buffer_destroy_bare(result);
        return NULL;
    }
    
    return result;
}

tsf_serial_in_man_state_t tsf_serial_in_man_get_state(tsf_serial_in_man_t *in_man) {
    return in_man->state;
}

void tsf_serial_in_man_set_state(tsf_serial_in_man_t *in_man,
                                 tsf_serial_in_man_state_t state) {
    in_man->state = state;
}

