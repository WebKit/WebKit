/*
 * Copyright (C) 2015 Filip Pizlo. All rights reserved.
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

tsf_destructor_t *tsf_destructor_create(tsf_type_t *type) {
    tsf_destructor_t *result;
    gpc_proto_t *proto;
    
    proto = tsf_gpc_code_gen_generate_destroyer(type);
    if (proto == NULL) {
        return NULL;
    }
    
    gpc_code_gen_debug("destroyer",proto);
    
    result = malloc(sizeof(tsf_destructor_t));
    if (result == NULL) {
        tsf_set_errno("Could not malloc tsf_destructor_t");
        gpc_proto_destroy(proto);
        return NULL;
    }
    
    result->destructor = gpc_program_from_proto(proto);
    gpc_proto_destroy(proto);
    
    if (result->destructor == NULL) {
        free(result);
        return NULL;
    }
    
    return result;
}

void tsf_destructor_destroy(tsf_destructor_t *destructor) {
    gpc_program_destroy(destructor->destructor);
    free(destructor);
}

void tsf_destructor_destruct(tsf_destructor_t *destructor,
                             void *object) {
    gpc_cell_t args[1];
    args[0] = (gpc_cell_t)object;
    gpc_program_run(destructor->destructor, NULL, args);
}

