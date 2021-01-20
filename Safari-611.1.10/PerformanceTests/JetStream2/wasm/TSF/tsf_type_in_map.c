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

#include "tsf_internal.h"
#include "tsf_format.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

static tsf_type_in_map_t empty_singleton;
static tsf_once_t empty_singleton_once_control = TSF_ONCE_INIT;

tsf_type_in_map_t *tsf_type_in_map_create() {
    return tsf_type_in_map_create_in(NULL);
}

tsf_type_in_map_t *tsf_type_in_map_create_in(void *region) {
    tsf_type_in_map_t *ret=tsf_cond_alloc(region,
                                          sizeof(tsf_type_in_map_t));
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_cond_alloc tsf_type_in_map_t");
        return NULL;
    }
    
    ret->region=region;
    ret->elements=NULL;
    ret->num_elements=0;
    ret->capacity=0;
    
    return ret;
}

tsf_type_in_map_t *tsf_type_in_map_from_type_out_map(
                                                tsf_type_out_map_t *type_map) {
    return tsf_type_in_map_from_type_out_map_in(type_map,NULL);
}

static int table_snatch_pair(tsf_type_t *type,
                             uint32_t *value,
                             void *arg) {
    tsf_type_in_map_t *types=arg;
    if (types->region!=NULL) {
        if (types->elements==NULL) {
            free(value);
            tsf_type_destroy(type);
            return TSF_ST_CONTINUE;
        }
        if (!tsf_region_add_cback(types->region,
                                  (tsf_region_cback_t)tsf_type_destroy,
                                  type)) {
            tsf_set_errno("Could not tsf_region_add_cback for "
                          "tsf_type_destroy");
            tsf_type_destroy(type);
            types->elements=NULL;   /* hackish signal that something is wrong. */
            return TSF_ST_CONTINUE;
        }
    }
    types->elements[*value]=type;
    free(value);
    return TSF_ST_CONTINUE;
}

tsf_type_in_map_t *tsf_type_in_map_from_type_out_map_in(
                                                tsf_type_out_map_t *type_map,
                                                void *region) {
    tsf_type_in_map_t *ret=tsf_cond_alloc(region,
                                          sizeof(tsf_type_in_map_t));
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_cond_alloc tsf_type_in_map_t");
        if (region==NULL) {
            tsf_type_out_map_destroy(type_map);
        }
        return NULL;
    }
    
    ret->region=region;
    
    ret->num_elements=type_map->num_entries;
    
    ret->elements=tsf_cond_alloc(region,
                                 sizeof(tsf_type_t*)*ret->num_elements);
    if (ret->elements==NULL) {
        tsf_set_errno("Could not tsf_cond_alloc array of tsf_type_t*");
        if (region==NULL) {
            tsf_type_out_map_destroy(type_map);
            free(ret);
        }
        return NULL;
    }
    
    tsf_st_foreach(type_map,(tsf_st_func_t)table_snatch_pair,ret);
    tsf_st_free_table(type_map);
    
    if (ret->elements==NULL) {
        /* something went wrong; this only happens in region allocation
         * case. */
        return NULL;
    }
        
    return ret;
}

static void init_empty_singleton() {
    empty_singleton.region=NULL;
    empty_singleton.elements=NULL;
    empty_singleton.num_elements=0;
    empty_singleton.capacity=0;
}

tsf_type_in_map_t *tsf_type_in_map_get_empty_singleton() {
    tsf_once(&empty_singleton_once_control,
             init_empty_singleton);

    return &empty_singleton;
}

tsf_type_in_map_t *tsf_type_in_map_clone(tsf_type_in_map_t *types) {
    return tsf_type_in_map_clone_in(types,NULL);
}

tsf_type_in_map_t *tsf_type_in_map_clone_in(tsf_type_in_map_t *types,
                                            void *region) {
    tsf_type_in_map_t *ret;
    uint32_t i;
    
    if (types==&empty_singleton) {
        return types;
    }
    
    ret=tsf_cond_alloc(region,sizeof(tsf_type_in_map_t));
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_cond_alloc tsf_type_in_map_t");
        return NULL;
    }
    
    ret->region=region;
    
    ret->num_elements=types->num_elements;
    ret->elements=tsf_cond_alloc(region,
                                 sizeof(tsf_type_t*)*ret->num_elements);
    if (ret->elements==NULL) {
        if (region!=NULL) {
            free(ret);
        }
        tsf_set_errno("Could not tsf_cond_alloc array of tsf_type_t*");
        return NULL;
    }
    
    for (i=0;i<ret->num_elements;++i) {
        if (region!=NULL) {
            if (!tsf_region_add_cback(region,
                                      (tsf_region_cback_t)tsf_type_destroy,
                                      types->elements[i])) {
                tsf_set_errno("Could not tsf_region_add_cback for "
                              "tsf_type_destroy");
                return tsf_false;
            }
        }
        ret->elements[i]=tsf_type_dup(types->elements[i]);
    }

    return ret;
}

void tsf_type_in_map_destroy(tsf_type_in_map_t *types) {
    uint32_t i;
    
    tsf_assert(types->region==NULL);
    
    if (types==&empty_singleton) {
        return;
    }
    
    for (i=0;i<types->num_elements;++i) {
        tsf_type_destroy(types->elements[i]);
    }
    if (types->elements!=NULL) {
        free(types->elements);
    }
    free(types);
}

tsf_bool_t tsf_type_in_map_prepare(tsf_type_in_map_t *types,
                                   uint32_t num) {
    if (types->num_elements+num <= types->capacity) {
        return tsf_true;
    }

    num = types->num_elements+num - types->capacity;

    if (types->region==NULL) {
        if (types->elements==NULL) {
            types->elements=malloc(sizeof(tsf_type_t*)*num);
            
            if (types->elements==NULL) {
                tsf_set_errno("Could not malloc tsf_type_t*");
                return tsf_false;
            }
        } else {
            tsf_type_t **new_array=
                realloc(types->elements,
                        sizeof(tsf_type_t*)*(types->capacity+num));
            
            if (new_array==NULL) {
                tsf_set_errno("Could not realloc array of tsf_type_t*");
                return tsf_false;
            }
            
            types->elements=new_array;
        }
    } else {
        tsf_type_t **new_array=
            tsf_region_alloc(types->region,
                             sizeof(tsf_type_t*)*(types->capacity+num));
        
        if (new_array==NULL) {
            tsf_set_errno("Could not tsf_region_alloc() array of "
                          "tsf_type_t*");
            return tsf_false;
        }
        
        memcpy(new_array,
               types->elements,
               sizeof(tsf_type_t*)*(types->num_elements));
        
        types->elements=new_array;
    }
    
    types->capacity+=num;
    
    return tsf_true;
}

tsf_bool_t tsf_type_in_map_append(tsf_type_in_map_t *types,
                                  tsf_type_t *type) {
    if (type==NULL) {
        return tsf_false;
    }
    
    if (!tsf_type_in_map_prepare(types,1)) {
        return tsf_false;
    }
    
    if (types->region!=NULL) {
        if (!tsf_region_add_cback(types->region,
                                  (tsf_region_cback_t)tsf_type_destroy,
                                  type)) {
            tsf_set_errno("Could not tsf_region_add_cback for "
                          "tsf_type_destroy");
            return tsf_false;
        }
    }
    
    types->elements[types->num_elements++]=tsf_type_dup(type);

    return tsf_true;
}

uint32_t tsf_type_in_map_get_num_types(tsf_type_in_map_t *types) {
    return types->num_elements;
}

tsf_type_t *tsf_type_in_map_get_type(tsf_type_in_map_t *types,
                                     uint32_t index) {
    return types->elements[index];
}

void tsf_type_in_map_truncate(tsf_type_in_map_t *types,
                              uint32_t new_size) {
    types->num_elements = new_size;
}

tsf_bool_t tsf_type_in_map_compare(tsf_type_in_map_t *a,
                                   tsf_type_in_map_t *b) {
    uint32_t i;
    
    if (a==b) {
        return tsf_true;
    }
    
    if (a->num_elements!=b->num_elements) {
        return tsf_false;
    }
    
    for (i=0;i<a->num_elements;++i) {
        if (!tsf_type_compare(a->elements[i],b->elements[i])) {
            return tsf_false;
        }
    }
    
    return tsf_true;
}

