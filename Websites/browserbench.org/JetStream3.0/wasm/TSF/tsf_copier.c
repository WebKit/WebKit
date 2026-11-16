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

tsf_copier_t *tsf_copier_create(tsf_type_t *dest,
                                tsf_type_t *src) {
    tsf_copier_t *result;
    gpc_proto_t *proto=
        tsf_gpc_code_gen_generate_converter(dest,src);
    
    if (proto==NULL) {
        return NULL;
    }
    
    gpc_code_gen_debug("converter",proto);
    
    result=malloc(sizeof(tsf_copier_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_copier_t");
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    result->copier = gpc_program_from_proto(proto);
    
    gpc_proto_destroy(proto);
    
    if (result->copier==NULL) {
        free(result);
        return NULL;
    }
    
    return result;
}

void tsf_copier_destroy(tsf_copier_t *copier) {
    gpc_program_destroy(copier->copier);
    free(copier);
}

void *tsf_copier_clone(tsf_copier_t *copier,void *src) {
    gpc_cell_t args[2];
    args[0]=(gpc_cell_t)NULL;
    args[1]=(gpc_cell_t)src;
    return (void*)gpc_program_run(copier->copier,NULL,args);
}

void *tsf_copier_clone_in(tsf_copier_t *copier,void *src,void *region) {
    gpc_cell_t args[2];
    args[0]=(gpc_cell_t)NULL;
    args[1]=(gpc_cell_t)src;
    return (void*)gpc_program_run(copier->copier,region,args);
}

tsf_bool_t tsf_copier_copy(tsf_copier_t *copier,
                           void *dest,
                           void *src,
                           void *region) {
    gpc_cell_t args[2];
    args[0]=(gpc_cell_t)dest;
    args[1]=(gpc_cell_t)src;
    return gpc_program_run(copier->copier,region,args)!=0?
               tsf_true:tsf_false;
}




