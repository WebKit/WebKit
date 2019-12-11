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

tsf_type_out_map_t *tsf_type_out_map_create() {
    tsf_type_out_map_t *ret = tsf_st_init_ptrtable();
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_st_init_ptrtable()");
        return NULL;
    }
    
    return ret;
}

static int table_destroy_pair(tsf_type_t *type,
                              uint32_t *value,
                              void *arg) {
    /* printf("Destroying type = %p\n", type); */
    tsf_type_destroy(type);
    free(value);
    return TSF_ST_CONTINUE;
}

void tsf_type_out_map_destroy(tsf_type_out_map_t *type_map) {
    tsf_st_foreach(type_map, (tsf_st_func_t)table_destroy_pair, NULL);
    tsf_st_free_table(type_map);
}

tsf_bool_t tsf_type_out_map_find_type_code(tsf_type_out_map_t *type_map,
                                           tsf_type_t *type,
                                           uint32_t *type_code) {
    uint32_t *tc_ptr;
    type = tsf_type_memo(type);
    if (type == NULL) {
        return tsf_false;
    }
    if (!tsf_st_lookup(type_map, (char*)type, (void**)&tc_ptr)) {
        tsf_set_error(TSF_E_INTERNAL,
                      "Given type was not found in type map even though "
                      "it was asserted that the type would be found");
        return tsf_false;
    }
    if (type_code!=NULL) {
        *type_code=*tc_ptr;
    }
    return tsf_true;
}

tsf_bool_t tsf_type_out_map_get_type_code(tsf_type_out_map_t *type_map,
                                          tsf_type_t *type,
                                          uint32_t *type_code,
                                          tsf_bool_t *created) {
    uint32_t *tc_ptr;
    
    type = tsf_type_memo(type);
    if (type == NULL) {
        return tsf_false;
    }
    
    if (tsf_st_lookup_ptrtable(type_map, type, (void**)&tc_ptr)) {
        if (type_code != NULL) {
            *type_code = *tc_ptr;
        }
        if (created != NULL) {
            *created = tsf_false;
        }
    } else {
        tc_ptr = malloc(sizeof(uint32_t));
        if (tc_ptr == NULL) {
            tsf_set_errno("Could not malloc type code");
            return tsf_false;
        }
        *tc_ptr = type_map->num_entries;
        /* printf("Adding type = %p\n", type); */
        if (tsf_st_add_direct(type_map,
                              (char*)tsf_type_dup(type),
                              tc_ptr) < 0) {
            tsf_set_errno("Could not tsf_st_add_direct()");
            free(tc_ptr);
            return tsf_false;
        }
        if (type_code != NULL) {
            *type_code = *tc_ptr;
        }
        if (created != NULL) {
            *created = tsf_true;
        }
    }
    
    return tsf_true;
}







