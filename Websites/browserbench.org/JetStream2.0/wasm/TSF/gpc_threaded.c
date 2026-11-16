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

#ifdef HAVE_ADDRESS_OF_LABEL

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

tsf_bool_t gpc_threaded_supported() {
    return tsf_true;
}

static void* const * instruction_dispatch = NULL;

static tsf_once_t once_control = TSF_ONCE_INIT;

static void init_instruction_dispatch() {
    gpc_threaded_run(NULL, NULL, 0);
}

static tsf_bool_t correct_branch(gpc_cell_t *cell,
                                 void *arg) {
    (*cell) = (gpc_cell_t)(((gpc_cell_t*) * cell) +
                           (size_t)arg);
    return tsf_true;
}

gpc_threaded_t *gpc_threaded_from_intable(gpc_intable_t *intable) {
    gpc_threaded_t *ret;
    gpc_cell_t *write;
    
    tsf_once(&once_control, init_instruction_dispatch);
    
    ret = malloc(sizeof(gpc_threaded_t));
    if (ret == NULL) {
        tsf_set_errno("Could not malloc gpc_threaded_t");
        return NULL;
    }
    
    ret->num_args = intable->num_args;
    ret->size = intable->size;
    ret->max_height = intable->max_height;
    
    ret->stream = malloc(sizeof(gpc_cell_t) * ret->size);
    if (ret->stream == NULL) {
        tsf_set_errno("Could not malloc stream for gpc_threaded_t");
        free(ret);
        return NULL;
    }
    
    memcpy(ret->stream, intable->stream, sizeof(gpc_cell_t) * ret->size);
    
    for (write = ret->stream;
         write < ret->stream + ret->size;) {
        uint32_t size = gpc_instruction_size(write);
        
        gpc_instruction_for_all_branches(
            write, correct_branch, (void*)(ret->stream - intable->stream));

        *write = (gpc_cell_t)instruction_dispatch[*write];
        
        write += size;
    }
    
    return ret;
}

void gpc_threaded_destroy(gpc_threaded_t *threaded) {
    free(threaded->stream);
    free(threaded);
}

#define INT_BEGIN(inst_name) \
    inst_name: /* printf("%s\n",#inst_name); */ ++inst; {

#define INT_END() \
    } goto *(void*)*inst;

#define INT_ADVANCE(amount) \
    (inst += (amount))

#define INT_GOTO(value) do {                      \
        void *tmp = (void*)value;                 \
        inst = tmp;                               \
        goto *(void*)*inst;                       \
    } while (0)

#include "gpc_int_common.h"

uintptr_t gpc_threaded_run(gpc_threaded_t *prog,
                           void *region,
                           gpc_cell_t *args) {
    static void *instructions[]={
#include "gpc_instruction_dispatch.gen"
    };

    uintptr_t *inst;
    uint8_t *buf = NULL,
            *cur = NULL,
            *end = NULL;
    uint32_t size = 0;
    tsf_bool_t keep = tsf_false;
    tsf_type_in_map_t *types = NULL;
    
    tsf_type_out_map_t *out_map = NULL;

    tsf_bool_t keep_region = tsf_false;
    
    int8_t i8_tmp;
    uint8_t ui8_tmp;
    int16_t i16_tmp;
    uint16_t ui16_tmp;
    int32_t i32_tmp;
    uint32_t ui32_tmp;
    int64_t i64_tmp;
    uint64_t ui64_tmp;
    float f_tmp;
    double d_tmp;
    
    uintptr_t *stack;
    uintptr_t *stack_top;
    
    uint32_t i;
    
    if (prog == NULL) {
        /* this is our special signal */
        instruction_dispatch = instructions;
        return 0;
    }
    
    stack = alloca(sizeof(uintptr_t) * prog->max_height);
    if (stack == NULL) {
        tsf_set_errno("Could not allocate stack");
        return 0;
    }
    
    stack_top = stack - 1;
    
    for (i = 0; i < prog->num_args; ++i) {
        INT_PUSH();
        INT_STACK(0) = args[i];
    }
    
    INT_GOTO(prog->stream);

#include "gpc_interpreter.gen"

bounds_error:
    tsf_set_error(TSF_E_PARSE_ERROR, "Bounds check failure");

failure:
    /* error! */
    if (keep) {
        free(buf);
    }
    if (keep_region) {
        tsf_region_free(region);
    }
    if (out_map != NULL) {
        tsf_type_out_map_destroy(out_map);
    }
    return 0;
}

#else

tsf_bool_t gpc_threaded_supported() {
    return tsf_false;
}

gpc_threaded_t *gpc_threaded_from_intable(gpc_intable_t *intable) {
    tsf_set_error(TSF_E_NOT_SUPPORTED,
                  "Threaded interpretation support was not compiled into "
                  "the TSF package.  This is because the address-of-label "
                  "extension was not supported by your compiler.");
    return NULL;
}

void gpc_threaded_destroy(gpc_threaded_t *threaded) {
    tsf_abort("Call to gpc_threaded_destroy() in a program that should "
              "never have been able to create a gpc_threaded_t, because "
              "threaded interpretation is not supported.");
}

uintptr_t gpc_threaded_run(gpc_threaded_t *prog,
                           void *region,
                           gpc_cell_t *args) {
    tsf_abort("Call to gpc_threaded_run() in a program that should "
              "never have been able to create a gpc_threaded_t, because "
              "threaded interpretation is not supported.");
}

#endif


