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

#include "gpc_internal.h"

#define C(exp) do {                         \
    if (!(exp)) {                           \
        goto failure;                       \
    }                                       \
} while (0)

typedef struct {
    gpc_cell_t *cell;
    intptr_t height;
} worklist_item_t;

#include "gpc_worklist.h"

static inline tsf_bool_t pop(worklist_t *worklist,
                             gpc_cell_t **cell,
                             intptr_t *height) {
    worklist_item_t *item=worklist_pop(worklist);
    if (item==NULL) {
        return tsf_false;
    }
    
    *cell=item->cell;
    *height=item->height;
    
    return tsf_true;
}

static inline tsf_bool_t push(worklist_t *worklist,
                              gpc_cell_t *cell,
                              intptr_t height) {
    worklist_item_t *item=worklist_push(worklist);
    if (item==NULL) {
        return tsf_false;
    }
    
    item->cell=cell;
    item->height=height;
    
    return tsf_true;
}

struct branch_action_closure {
    worklist_t *worklist;
    intptr_t height;
};

static tsf_bool_t branch_action(gpc_cell_t *cell,
                                void *arg) {
    struct branch_action_closure *closure=arg;
    return push(closure->worklist,(gpc_cell_t*)*cell,closure->height);
}

tsf_st_table *gpc_intable_get_stack_heights(gpc_intable_t *prog,
                                            intptr_t *max_height) {
    tsf_st_table *ret=tsf_st_init_ptrtable();
    gpc_cell_t *cur;
    intptr_t height;
    worklist_t worklist;
    
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_st_init_numtable()");
        return NULL;
    }
    
    if (!worklist_init(&worklist)) {
        tsf_st_free_table(ret);
        return NULL;
    }
    
    C(push(&worklist,
           gpc_intable_get_stream(prog),
           gpc_intable_get_num_args(prog)));
    
    if (max_height!=NULL) {
        *max_height=0;
    }
    
    while (pop(&worklist,&cur,&height)) {
        intptr_t got_height;
        
        if (tsf_st_lookup(ret,(char*)cur,(void**)&got_height)) {
            if (got_height != height) {
                tsf_set_error(TSF_E_SEMANTIC,
                              "The stack height is different on two "
                              "different approaches to the same piece "
                              "of code");
                goto failure;
            }
            continue;   /* we've seen it so we don't need to do anything
                         * else. */
        }
        
        if (tsf_st_add_direct(ret,(char*)cur,(void*)height)<0) {
            tsf_set_errno("Could not tsf_st_add_direct()");
            goto failure;
        }
        
        if (max_height!=NULL && height>*max_height) {
            *max_height=height;
        }
        
        height+=gpc_instruction_stack_effects(*cur);
        
        {
            struct branch_action_closure closure;
            closure.worklist=&worklist;
            closure.height=height;
            C(gpc_instruction_for_all_branches(cur,branch_action,&closure));
        }
        
        if (gpc_instruction_is_continuous(*cur)) {
            C(push(&worklist,cur+gpc_instruction_size(cur),height));
        }
    }
    
    worklist_done(&worklist);
    
    return ret;
    
failure:
    tsf_st_free_table(ret);
    worklist_done(&worklist);
    
    return NULL;
}


