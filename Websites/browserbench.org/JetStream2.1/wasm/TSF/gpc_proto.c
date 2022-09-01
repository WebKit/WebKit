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

#include "gpc_internal.h"
#include "tsf_format.h"

gpc_proto_t *gpc_proto_create(uint32_t num_args) {
    gpc_proto_t *ret=malloc(sizeof(gpc_proto_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc gpc_proto_t");
        return NULL;
    }
    
    ret->num_args=num_args;
    ret->stream=NULL;
    ret->size=0;
    
    return ret;
}

void gpc_proto_destroy(gpc_proto_t *proto) {
    if (proto->stream!=NULL) {
        free(proto->stream);
    }
    free(proto);
}

tsf_bool_t gpc_proto_reserve(gpc_proto_t *proto,
                             uint32_t num_slots) {
    if (proto->stream==NULL) {
        proto->stream=malloc(sizeof(gpc_cell_t)*num_slots);
        if (proto->stream==NULL) {
            tsf_set_errno("Could not malloc gpc_cell_t array");
            return tsf_false;
        }
    } else {
        gpc_cell_t *new_array=
            realloc(proto->stream,
                    sizeof(gpc_cell_t)
                    * (proto->size+num_slots));
        if (new_array==NULL) {
            tsf_set_errno("Could not realloc gpc_cell_t array");
            return tsf_false;
        }
        proto->stream=new_array;
    }
    
    return tsf_true;
}

tsf_bool_t gpc_proto_append(gpc_proto_t *proto,
                            gpc_cell_t inst,
                            ...) {
    va_list lst;
    uint32_t size=gpc_instruction_static_size(inst);
    
    va_start(lst,inst);

    if (size==UINT32_MAX) {
        gpc_cell_t len=
            va_arg(lst,gpc_cell_t);

        if (!gpc_proto_reserve(proto,2+len)) {
            return tsf_false;
        }
        
        proto->stream[proto->size++]=inst;
        proto->stream[proto->size++]=len;
        
        while (len-->0) {
            proto->stream[proto->size++]=
                va_arg(lst,gpc_cell_t);
        }
        
        return tsf_true;
    }
    
    if (!gpc_proto_reserve(proto,size)) {
        return tsf_false;
    }
    
    proto->stream[proto->size++]=inst;
    
    while (--size>0) {
        proto->stream[proto->size++]=
            va_arg(lst,gpc_cell_t);
    }
    
    return tsf_true;
}

tsf_bool_t gpc_proto_append_tableset_local_to_field(gpc_proto_t *proto,
                                                    gpc_cell_t offset,
                                                    gpc_cell_t num,
                                                    const gpc_cell_t *args) {
    uint32_t i;

    if (!gpc_proto_reserve(proto,3+num)) {
        return tsf_false;
    }
    
    proto->stream[proto->size++]=GPC_I_TABLESET_LOCAL_TO_FIELD;
    proto->stream[proto->size++]=offset;
    proto->stream[proto->size++]=num;
    
    for (i=0;i<num;++i) {
        proto->stream[proto->size++]=args[i];
    }
    
    return tsf_true;
}

tsf_bool_t gpc_proto_append_tableset_field_to_field(gpc_proto_t *proto,
                                                    gpc_cell_t dest_offset,
                                                    gpc_cell_t src_offset,
                                                    gpc_cell_t num,
                                                    const gpc_cell_t *args) {
    uint32_t i;

    if (!gpc_proto_reserve(proto,4+num)) {
        return tsf_false;
    }
    
    proto->stream[proto->size++]=GPC_I_TABLESET_FIELD_TO_FIELD;
    proto->stream[proto->size++]=dest_offset;
    proto->stream[proto->size++]=src_offset;
    proto->stream[proto->size++]=num;
    
    for (i=0;i<num;++i) {
        proto->stream[proto->size++]=args[i];
    }
    
    return tsf_true;
}

tsf_bool_t gpc_proto_append_tablejump_local(gpc_proto_t *proto,
                                            gpc_cell_t num,
                                            const gpc_cell_t *args) {
    uint32_t i;

    if (!gpc_proto_reserve(proto,2+num)) {
        return tsf_false;
    }
    
    proto->stream[proto->size++]=GPC_I_TABLEJUMP_LOCAL;
    proto->stream[proto->size++]=num;
    
    for (i=0;i<num;++i) {
        proto->stream[proto->size++]=args[i];
    }
    
    return tsf_true;
}

tsf_bool_t gpc_proto_append_tablejump_field(gpc_proto_t *proto,
                                            gpc_cell_t offset,
                                            gpc_cell_t num,
                                            const gpc_cell_t *args) {
    uint32_t i;

    if (!gpc_proto_reserve(proto,3+num)) {
        return tsf_false;
    }
    
    proto->stream[proto->size++]=GPC_I_TABLEJUMP_FIELD;
    proto->stream[proto->size++]=offset;
    proto->stream[proto->size++]=num;
    
    for (i=0;i<num;++i) {
        proto->stream[proto->size++]=args[i];
    }
    
    return tsf_true;
}

void gpc_proto_print(gpc_proto_t *proto,
                     FILE *out) {
    uint32_t i;
    for (i = 0; i < proto->size;) {
        uint32_t size = gpc_instruction_size(proto->stream + i);
        if (proto->stream[i] == GPC_I_LABEL) {
            fprintf(out, fui64 ":", (uint64_t)proto->stream[i + 1]);
        } else {
            uint32_t j;
            const char *name=
                gpc_instruction_to_string(proto->stream[i]);
            if (strlen(name) < 35) {
                for (j = 0; j < 35 - strlen(name); ++j) {
                    fprintf(out, " ");
                }
            }
            fprintf(out, "%s", name);
            for (j = 1; j < size; ++j) {
                fprintf(out, "\t" fui64, (uint64_t)proto->stream[i + j]);
            }
        }
        fprintf(out, "\n");
        i += size;
    }
}

