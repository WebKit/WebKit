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
#include "tsf_serial_protocol.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

/* FIXME: add an recovery helper mode, where after some number of bytes X
 * such that X/T > C, where T is the byte size of our current type map,
 * we reset types.  Also add an extended reset types command, which
 * includes a longer magic string. */

static tsf_bool_t write_command(tsf_writer_t writer,
                                void *arg,
                                uint8_t command) {
    return writer(arg, &command, sizeof(command));
}

static tsf_bool_t write_reset(tsf_writer_t writer,
                              void *arg) {
    return writer(arg, TSF_SP_MAGIC_CODE, TSF_SP_MAGIC_LEN);
}

tsf_serial_out_man_t *tsf_serial_out_man_create(tsf_writer_t writer,
                                                void *arg) {
    tsf_serial_out_man_t *ret=malloc(sizeof(tsf_serial_out_man_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_serial_out_man_t structure.");
        return NULL;
    }
    
    ret->types=tsf_type_out_map_create();
    if (ret->types==NULL) {
        free(ret);
        return NULL;
    }
    
    ret->writer=writer;
    ret->arg=arg;
    
    if (!write_reset(writer,arg)) {
        tsf_type_out_map_destroy(ret->types);
        free(ret);
        return NULL;
    }
    
    return ret;
}

void tsf_serial_out_man_destroy(tsf_serial_out_man_t *out_man) {
    tsf_type_out_map_destroy(out_man->types);
    free(out_man);
}

tsf_bool_t tsf_serial_out_man_reset_types(tsf_serial_out_man_t *out_man) {
    tsf_type_out_map_t *new_types;
    
    new_types=tsf_type_out_map_create();
    if (new_types==NULL) {
        return tsf_false;
    }
    
    if (!write_reset(out_man->writer,
                     out_man->arg)) {
        tsf_type_out_map_destroy(new_types);
        return tsf_false;
    }
    
    tsf_type_out_map_destroy(out_man->types);
    out_man->types=new_types;
    
    return tsf_true;
}

tsf_bool_t tsf_serial_out_man_write_type(tsf_writer_t writer,
                                         void *arg,
                                         tsf_type_t *type) {
    if (!write_command(writer,arg,TSF_SP_C_NEW_TYPE)) {
        return tsf_false;
    }
    return tsf_type_write(type,writer,arg);
}

tsf_bool_t tsf_serial_out_man_write(tsf_serial_out_man_t *out_man,
                                    tsf_buffer_t *buf) {
    uint32_t i;
    tsf_bool_t created;
    uint32_t type_code;
    
    if (!tsf_type_out_map_get_type_code(out_man->types,
                                        buf->type,
                                        NULL,
                                        &created)) {
        goto error;
    }
    
    if (created) {
        if (!tsf_serial_out_man_write_type(out_man->writer,
                                           out_man->arg,
                                           buf->type)) {
            goto error;
        }
    }
    
    for (i = 0; i < tsf_type_in_map_get_num_types(buf->types); ++i) {
        if (!tsf_type_out_map_get_type_code(
		out_man->types,
		tsf_type_in_map_get_type(buf->types, i),
		NULL,
		&created)) {
            goto error;
        }
        
        if (created) {
            if (!tsf_serial_out_man_write_type(
                     out_man->writer,
                     out_man->arg,
                     tsf_type_in_map_get_type(buf->types, i))) {
                goto error;
            }
        }
    }
    
    if (!tsf_type_out_map_get_type_code(out_man->types,
                                        buf->type,
                                        &type_code,
                                        NULL)) {
        goto error;
    }
    
    if (type_code < TSF_SP_C_CF_DATA_BIT && buf->size < 256) {
        if (!write_command(out_man->writer, out_man->arg, TSF_SP_C_CF_DATA_BIT | type_code)) {
            goto error;
        }
        
        if (!tsf_buffer_write_small_with_type_code(buf,
                                                   out_man->types,
                                                   out_man->writer,
                                                   out_man->arg)) {
            goto error;
        }
    } else {
        if (!write_command(out_man->writer, out_man->arg, TSF_SP_C_DATA)) {
            goto error;
        }
        
        if (!tsf_buffer_write_simple(buf,
                                     type_code,
                                     out_man->types,
                                     out_man->writer,
                                     out_man->arg)) {
            goto error;
        }
    }

    return tsf_true;

error:
    return tsf_false;
}


