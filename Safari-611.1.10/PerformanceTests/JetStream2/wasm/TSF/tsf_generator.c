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
#include "gpc.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

tsf_genrtr_t *tsf_generator_create(tsf_type_t *type) {
    tsf_genrtr_t *result;
    gpc_proto_t *proto;
    
    proto=tsf_gpc_code_gen_generate_generator(type);
    if (proto==NULL) {
        return NULL;
    }
    
    gpc_code_gen_debug("generator",proto);
    
    result=malloc(sizeof(tsf_genrtr_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_genrtr_t");
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    result->type=tsf_type_dup(type);
    result->generator=
	gpc_program_from_proto(proto);
    
    gpc_proto_destroy(proto);
    
    if (result->generator==NULL) {
        free(result);
        return NULL;
    }
    
    return result;
}

void tsf_generator_destroy(tsf_genrtr_t *gen) {
    gpc_program_destroy(gen->generator);
    tsf_type_destroy(gen->type);
    free(gen);
}

tsf_buffer_t *tsf_generator_generate(tsf_genrtr_t *gen,
                                     void *data) {
    gpc_cell_t args[2];
    tsf_buffer_t *buffer;
    
    buffer = tsf_buffer_create_bare();
    if (buffer == NULL) {
        return NULL;
    }
    
    args[0] = (gpc_cell_t)buffer;
    args[1] = (gpc_cell_t)data;
    if (!gpc_program_run(gen->generator, NULL, args)) {
        tsf_buffer_destroy_bare(buffer);
        return NULL;
    }
    
    buffer->type = tsf_type_dup(gen->type);
    
    return buffer;
}

tsf_buffer_t *tsf_generator_generate_in(tsf_genrtr_t *gen,
                                        void *data,
                                        void *region) {
    gpc_cell_t args[2];
    tsf_buffer_t *buffer;
    
    if (region == NULL) {
        return tsf_generator_generate(gen, data);
    }
    
    buffer = tsf_buffer_create_bare_in(region);
    if (buffer == NULL) {
        return NULL;
    }
    
    args[0] = (gpc_cell_t)buffer;
    args[1] = (gpc_cell_t)data;
    if (!gpc_program_run(gen->generator, region, args)) {
        return NULL;
    }
    
    if (!tsf_region_add_cback(region, (tsf_region_cback_t)tsf_type_destroy, gen->type)) {
        return NULL;
    }
    buffer->type = tsf_type_dup(gen->type);
    
    return buffer;
}

tsf_bool_t tsf_generator_generate_existing_buffer(tsf_genrtr_t *gen,
                                                  void *data,
                                                  tsf_buffer_t *buf) {
    gpc_cell_t args[2];
    
    args[0] = (gpc_cell_t)buf;
    args[1] = (gpc_cell_t)data;
    if (!gpc_program_run(gen->generator, NULL, args))
        return tsf_false;
    
    buf->type = tsf_type_dup(gen->type);
    return tsf_true;
}

