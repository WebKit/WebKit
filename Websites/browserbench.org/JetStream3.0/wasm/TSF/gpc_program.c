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

gpc_program_t *gpc_program_from_proto(gpc_proto_t *proto) {
    gpc_intable_t *intable;
    gpc_program_t *ret;
    
    intable = gpc_intable_from_proto(proto);
    
    if (intable == NULL) {
        return NULL;
    }
    
    ret = gpc_program_from_intable(intable);

    gpc_intable_destroy(intable);
    
    return ret;
}

gpc_program_t *gpc_program_from_intable(gpc_intable_t *intable) {
    gpc_program_t *ret;

    ret = malloc(sizeof(gpc_program_t));
    if (ret == NULL) {
        tsf_set_errno("Could not malloc gpc_program_t");
        return NULL;
    }
    
    if (gpc_threaded_supported()) {
        ret->payload=gpc_threaded_from_intable(intable);
        if (ret->payload==NULL) {
            free(ret);
            return NULL;
        }
        ret->runner=(gpc_program_runner_t)gpc_threaded_run;
        ret->destroyer=(gpc_program_destroyer_t)gpc_threaded_destroy;
    } else {
        ret->payload=gpc_intable_clone(intable);
        if (ret->payload==NULL) {
            free(ret);
            return NULL;
        }
        ret->runner=(gpc_program_runner_t)gpc_intable_run;
        ret->destroyer=(gpc_program_destroyer_t)gpc_intable_destroy;
    }
    
    return ret;
}

void gpc_program_destroy(gpc_program_t *prog) {
    (prog->destroyer)(prog->payload);
    free(prog);
}

uintptr_t gpc_program_run(gpc_program_t *prog,
                          void *region,
                          gpc_cell_t *args) {
    return (prog->runner)(prog->payload, region, args);
}

