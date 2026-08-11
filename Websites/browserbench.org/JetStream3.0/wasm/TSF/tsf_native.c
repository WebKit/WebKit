/*
 * Copyright (C) 2003, 2004, 2005, 2014, 2015 Filip Pizlo. All rights reserved.
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
#include <string.h>

tsf_bool_t tsf_native_type_has_struct_mapping(tsf_type_t *type) {
    /* printf("in has struct mapping: type = %p\n",type); */
    switch (type->kind_code) {
    case TSF_TK_ARRAY: {
        /* an array has a struct mapping if its elements have
         * struct mappings */
        return tsf_native_type_has_struct_mapping(
            type->u.a.element_type);
    }
    case TSF_TK_STRUCT: {
        uint32_t i;
        if (!type->u.s.has_size) {
            return tsf_false;
        }
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            if (!tsf_struct_type_get_element(type, i)->has_offset) {
                return tsf_false;
            }
            if (!tsf_native_type_has_struct_mapping(
                    tsf_struct_type_get_element(type, i)->type)) {
                return tsf_false;
            }
        }
        break;
    }
    case TSF_TK_CHOICE: {
        uint32_t i;
        if (!type->u.h.has_mapping) {
            return tsf_false;
        }
        for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
            if (!tsf_native_type_has_struct_mapping(
                    tsf_choice_type_get_element(type, i)->type)) {
                return tsf_false;
            }
        }
        break;
    }
    default:
        /* primitives, bits, and voids have struct mappings */
        break;
    }
    
    return tsf_true;
}

size_t tsf_native_type_get_size(tsf_type_t *type) {
    /* printf("in get size: type = %p\n",type); */
    tsf_assert(tsf_native_type_has_struct_mapping(type));
    
    switch (type->kind_code) {
    case TSF_TK_ARRAY:
        if (type->u.a.element_type->kind_code == TSF_TK_BIT) {
            return sizeof(tsf_native_bitvector_t);
        }
        if (type->u.a.element_type->kind_code == TSF_TK_VOID) {
            return sizeof(tsf_native_void_array_t);
        }
        return sizeof(tsf_native_array_t);
    case TSF_TK_STRUCT:
        return type->u.s.size;
    case TSF_TK_BIT:
        return sizeof(tsf_bool_t);
    case TSF_TK_VOID:
        return 0;
    case TSF_TK_CHOICE:
        return type->u.h.size;
    case TSF_TK_STRING:
        return sizeof(char*);
    case TSF_TK_ANY:
        return sizeof(tsf_buffer_t*);
    default:
        return tsf_primitive_type_kind_size_of(type->kind_code);
    }
}

static size_t saturated_plus(size_t a, size_t b) {
    size_t result = a + b;
    if (result < a || result < b) {
        result = UINTPTR_MAX;
    }
    return result;
}

static void extra_size_of_indirect(tsf_type_t *type,
                                   size_t *min,
                                   size_t *max) {
    switch (type->kind_code) {
    case TSF_TK_ARRAY:
    case TSF_TK_ANY: {
        *max = UINTPTR_MAX;
        break;
    }
        
    case TSF_TK_STRUCT: {
        uint32_t i;
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            extra_size_of_indirect(tsf_struct_type_get_element(type, i)->type, min, max);
        }
        break;
    }
        
    case TSF_TK_CHOICE: {
        uint32_t i;
        size_t my_min = 0, my_max = 0;
        for (i = 0; i < tsf_choice_type_get_num_elements(type); ++i) {
            tsf_type_t *ele_type;
            size_t this_min, this_max;
            
            ele_type = tsf_choice_type_get_element(type, i)->type;
            this_min = 0;
            this_max = 0;
            
            if (tsf_native_choice_type_is_in_place(type)) {
                extra_size_of_indirect(ele_type, &this_min, &this_max);
            } else {
                tsf_native_type_get_total_size_bounds(ele_type, &this_min, &this_max);
            }
            
            my_min = tsf_max(my_min, this_min);
            my_max = tsf_max(my_max, this_max);
        }
        *min = saturated_plus(*min, my_min);
        *max = saturated_plus(*max, my_max);
        break;
    }
        
    default:
        break;
    }
}

void tsf_native_type_get_total_size_bounds(tsf_type_t *type,
                                           size_t *min,
                                           size_t *max) {
    size_t base_size, my_min, my_max;
    
    base_size = tsf_native_type_get_size(type);
    
    my_min = 0;
    my_max = 0;
    extra_size_of_indirect(type, &my_min, &my_max);
    
    *min = saturated_plus(base_size, my_min);
    *max = saturated_plus(base_size, my_max);
}

static tsf_bool_t native_type_can_blit_recurse(tsf_type_t *dest,
                                               tsf_type_t *src,
                                               size_t dest_size) {
    uint32_t i;
    size_t sub_dest_size;
    if (dest->kind_code!=src->kind_code) {
        return tsf_false;
    }
    if (dest_size==0) {
        dest_size=tsf_native_type_get_size(dest);
    }
    if (tsf_native_type_get_size(src) > dest_size) {
        return tsf_false;
    }
    switch (dest->kind_code) {
        case TSF_TK_ARRAY:
        case TSF_TK_STRING:
        case TSF_TK_ANY:
            return tsf_false;
        case TSF_TK_CHOICE:
            if (tsf_choice_type_get_num_elements(src) >
                tsf_choice_type_get_num_elements(dest)) {
                return tsf_false;
            }
            if ((tsf_choice_type_has_non_void(src) &&
                 src->u.h.data_offset != dest->u.h.data_offset) ||
                src->u.h.value_offset != dest->u.h.value_offset) {
                return tsf_false;
            }
            if (dest->u.h.value_offset > dest->u.h.data_offset) {
                sub_dest_size=dest->u.h.value_offset-dest->u.h.data_offset;
            } else {
                sub_dest_size=dest->u.h.size-dest->u.h.data_offset;
            }
            for (i=0;i<tsf_choice_type_get_num_elements(src);++i) {
                tsf_named_type_t *sn=tsf_choice_type_get_element(src,i);
                tsf_named_type_t *dn=tsf_choice_type_get_element(dest,i);
                if (strcmp(sn->name,dn->name)) {
                    return tsf_false;
                }
                if (!native_type_can_blit_recurse(dn->type,
                                                  sn->type,
                                                  sub_dest_size)) {
                    return tsf_false;
                }
		if (dn->type->kind_code!=TSF_TK_VOID &&
		    (!tsf_native_choice_type_is_in_place(dest) ||
		     !tsf_native_choice_type_is_in_place(src))) {
		    return tsf_false;
		}
            }
            break;
        case TSF_TK_STRUCT:
            if (dest->u.s.constructor != NULL ||
                dest->u.s.pre_destructor != NULL) {
                return tsf_false;
            }
            
            for (i=0;i<tsf_struct_type_get_num_elements(dest);++i) {
                tsf_named_type_t *dn=tsf_struct_type_get_element(dest,i);
                tsf_named_type_t *sn;
                
                if (tsf_type_get_static_size(dn->type)==0) {
                    continue;
                }
                
                sn=tsf_struct_type_find_node(src,dn->name);
                if (sn==NULL) {
                    return tsf_false;
                }
                
                if (!native_type_can_blit_recurse(dn->type,
                                                  sn->type,
                                                  0)) {
                    return tsf_false;
                }
                
                if (dn->offset != sn->offset) {
                    return tsf_false;
                }
            }
            break;
        default:
            break;
    }
    
    return tsf_true;
}

tsf_bool_t tsf_native_type_can_blit(tsf_type_t *dest,
                                    tsf_type_t *src) {
    if (!tsf_native_type_has_struct_mapping(dest) ||
        !tsf_native_type_has_struct_mapping(src)) {
        return tsf_false;
    }
    
    return native_type_can_blit_recurse(dest,src,0);
}

size_t tsf_native_named_type_get_offset(tsf_named_type_t *node) {
    return node->offset;
}

tsf_bool_t tsf_native_struct_type_map(tsf_type_t **type,
                                      const char *name,
                                      size_t offset) {
    tsf_type_t *ret;
    tsf_named_type_t *n;
    /* uint32_t i; */
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    n = tsf_struct_type_find_node(ret,name);
    
    if (n == NULL) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }

    /* Ages ago, we used to verify the struct mapping here by examining all offsets
     * and the size. This was slow and it was also not particularly effective at
     * catching bugs. So, we don't do that anymore. */
    
    n->has_offset = tsf_true;
    n->offset = offset;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_native_struct_type_set_constructor(tsf_type_t **type,
                                                  void (*cons)(void*)) {
    tsf_type_t *ret=tsf_type_own_begin(*type);
    if (ret==NULL) {
	return tsf_false;
    }

    ret->u.s.constructor = cons;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_native_struct_type_set_pre_destructor(tsf_type_t **type,
                                                     void (*pre_dest)(void*)) {
    tsf_type_t *ret=tsf_type_own_begin(*type);
    if (ret==NULL) {
	return tsf_false;
    }

    ret->u.s.pre_destructor = pre_dest;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_native_struct_type_set_destructor(tsf_type_t **type,
                                                 void (*dest)(void*)) {
    tsf_type_t *ret=tsf_type_own_begin(*type);
    if (ret==NULL) {
	return tsf_false;
    }

    ret->u.s.destructor = dest;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

void (*tsf_native_struct_type_get_constructor(tsf_type_t *type))(void*) {
    return type->u.s.constructor;
}

void (*tsf_native_struct_type_get_pre_destructor(tsf_type_t *type))(void*) {
    return type->u.s.pre_destructor;
}

void (*tsf_native_struct_type_get_destructor(tsf_type_t *type))(void*) {
    return type->u.s.destructor;
}

tsf_bool_t tsf_native_struct_type_set_size(tsf_type_t **type,
                                           size_t size) {
    tsf_type_t *ret=tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    ret->u.s.has_size=tsf_true;
    ret->u.s.size=size;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_native_choice_type_map(tsf_type_t **type,
				      tsf_bool_t in_place,
                                      size_t value_offset,
                                      size_t data_offset,
                                      size_t size) {
    tsf_type_t *ret=tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    ret->u.h.has_mapping = tsf_true;
    ret->u.h.in_place = in_place;
    ret->u.h.value_offset = value_offset;
    ret->u.h.data_offset = data_offset;
    ret->u.h.size = size;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_native_type_produce_some_mapping(tsf_type_t **type,
						tsf_bool_t do_align) {
    tsf_type_t *ret = tsf_type_own_begin(*type);
    uint32_t i;
    size_t total_size;
    
    if (ret == NULL) {
        return tsf_false;
    }
    
    switch ((*type)->kind_code) {
    case TSF_TK_ARRAY:
        if (!tsf_native_type_produce_some_mapping(&ret->u.a.element_type,
                                                  do_align)) {
            tsf_type_own_abort(*type, ret);
            return tsf_false;
        }
        break;
    case TSF_TK_STRUCT:
        total_size = 0;
            
        for (i = 0;
             i < tsf_type_table_get_num_elements(ret->u.s.tt);
             ++i) {
            tsf_named_type_t *n =
                tsf_type_table_find_by_index(ret->u.s.tt, i);
                
            if (!tsf_native_type_produce_some_mapping(&n->type,
                                                      do_align)) {
                tsf_type_own_abort(*type, ret);
                return tsf_false;
            }
                
            if (do_align) {
                n->offset = TSF_ALIGN(total_size);
            } else {
                n->offset = total_size;
            }
            n->has_offset = tsf_true;
                
            total_size = n->offset + tsf_native_type_get_size(n->type);
        }
            
        ret->u.s.size = total_size;
        ret->u.s.has_size = tsf_true;
        break;
    case TSF_TK_CHOICE:
        ret->u.h.in_place = tsf_true;
	    
        total_size = 0;
            
        for (i = 0;
             i < tsf_type_table_get_num_elements(ret->u.h.tt);
             ++i) {
            tsf_named_type_t *n =
                tsf_type_table_find_by_index(ret->u.h.tt,i);
                
            size_t size;
                
            if (!tsf_native_type_produce_some_mapping(&n->type,
                                                      do_align)) {
                tsf_type_own_abort(*type, ret);
                return tsf_false;
            }
                
            size = tsf_native_type_get_size(n->type);
                
            if (size > total_size) {
                total_size = size;
            }
        }

        ret->u.h.value_offset = 0;

        if (do_align) {
            ret->u.h.data_offset = TSF_ALIGN(sizeof(tsf_choice_t));
        } else {
            ret->u.h.data_offset = sizeof(tsf_choice_t);
        }

        ret->u.h.size = ret->u.h.data_offset + total_size;
        ret->u.h.has_mapping = tsf_true;
        break;
    default:
        break;
    }
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_type_t *tsf_native_struct_type_create(size_t size) {
    tsf_type_t *result = tsf_type_create(TSF_TK_STRUCT);
    if (result == NULL) {
        return NULL;
    }
    
    if (!tsf_native_struct_type_set_size(&result, size)) {
        tsf_type_destroy(result);
        return NULL;
    }
    
    return result;
}

tsf_bool_t tsf_native_struct_type_append(tsf_type_t **type,
                                         const char *name,
                                         tsf_type_t *ele_type,
                                         size_t offset) {
    if (!tsf_struct_type_append(type, name, ele_type)) {
        return tsf_false;
    }
    
    if (!tsf_native_struct_type_map(type, name, offset)) {
        tsf_struct_type_remove(type, name);
        return tsf_false;
    }

    return tsf_true;
}

tsf_type_t *tsf_native_choice_type_create(tsf_bool_t in_place,
                                          size_t value_offset,
                                          size_t data_offset,
                                          size_t size) {
    tsf_type_t *result = tsf_type_create(TSF_TK_CHOICE);
    if (result == NULL) {
        return NULL;
    }
    
    if (!tsf_native_choice_type_map(&result, in_place, value_offset, data_offset, size)) {
        tsf_type_destroy(result);
        return NULL;
    }
    
    return result;
}

tsf_bool_t tsf_native_choice_type_is_in_place(tsf_type_t *type) {
    return type->u.h.in_place;
}

size_t tsf_native_choice_type_get_data_offset(tsf_type_t *type) {
    return type->u.h.data_offset;
}

size_t tsf_native_choice_type_get_value_offset(tsf_type_t *type) {
    return type->u.h.value_offset;
}

size_t tsf_native_choice_type_get_size(tsf_type_t *type) {
    return type->u.h.size;
}
