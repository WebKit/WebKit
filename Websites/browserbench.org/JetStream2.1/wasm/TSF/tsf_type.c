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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#ifdef DEBUG_TYPE_ALLOC
static int64_t type_balance = 0;

int64_t tsf_type_get_balance() {
    return type_balance;
}

#define increment_type_balance() do {\
    type_balance++;\
} while(0)

#define decrement_type_balance() do {\
    type_balance--;\
} while(0)

#else

#define increment_type_balance() do {\
} while(0)

#define decrement_type_balance() do {\
} while(0)

#endif

static tsf_type_t void_type,
                  int8_type,
                  uint8_type,
                  int16_type,
                  uint16_type,
                  int32_type,
                  uint32_type,
                  int64_type,
                  uint64_type,
                  integer_type,
                  long_type,
                  float_type,
                  double_type,
                  bit_type,
                  string_type,
                  any_type;

static tsf_st_table *memo_table;
static tsf_mutex_t memo_lock;

static void init_basic_type(tsf_type_t *type) {
    type->is_dynamic = tsf_false;
    type->comment = NULL;
    tsf_atomic_count_init(&type->ref_count, 1);
    type->byte_size = 1;
    type->name = NULL;
    type->version = 0;
    type->memo_master = type;
}

static void init_types(void) {
    tsf_assert(memo_table = tsf_st_init_typetable());
    tsf_mutex_init(&memo_lock);
    
    void_type.kind_code = TSF_TK_VOID;
    void_type.hash_code = TSF_TK_VOID;
    init_basic_type(&void_type);
    
    int8_type.kind_code = TSF_TK_INT8;
    int8_type.hash_code = TSF_TK_INT8;
    init_basic_type(&int8_type);
    
    uint8_type.kind_code = TSF_TK_UINT8;
    uint8_type.hash_code = TSF_TK_UINT8;
    init_basic_type(&uint8_type);
    
    int16_type.kind_code = TSF_TK_INT16;
    int16_type.hash_code = TSF_TK_INT16;
    init_basic_type(&int16_type);
    
    uint16_type.kind_code = TSF_TK_UINT16;
    uint16_type.hash_code = TSF_TK_UINT16;
    init_basic_type(&uint16_type);
    
    int32_type.kind_code = TSF_TK_INT32;
    int32_type.hash_code = TSF_TK_INT32;
    init_basic_type(&int32_type);
    
    uint32_type.kind_code = TSF_TK_UINT32;
    uint32_type.hash_code = TSF_TK_UINT32;
    init_basic_type(&uint32_type);
    
    int64_type.kind_code = TSF_TK_INT64;
    int64_type.hash_code = TSF_TK_INT64;
    init_basic_type(&int64_type);
    
    uint64_type.kind_code = TSF_TK_UINT64;
    uint64_type.hash_code = TSF_TK_UINT64;
    init_basic_type(&uint64_type);
    
    integer_type.kind_code = TSF_TK_INTEGER;
    integer_type.hash_code = TSF_TK_INTEGER;
    init_basic_type(&integer_type);
    
    long_type.kind_code = TSF_TK_LONG;
    long_type.hash_code = TSF_TK_LONG;
    init_basic_type(&long_type);
    
    float_type.kind_code = TSF_TK_FLOAT;
    float_type.hash_code = TSF_TK_FLOAT;
    init_basic_type(&float_type);
    
    double_type.kind_code = TSF_TK_DOUBLE;
    double_type.hash_code = TSF_TK_DOUBLE;
    init_basic_type(&double_type);
    
    bit_type.kind_code = TSF_TK_BIT;
    bit_type.hash_code = TSF_TK_BIT;
    init_basic_type(&bit_type);
    
    string_type.kind_code = TSF_TK_STRING;
    string_type.hash_code = TSF_TK_STRING;
    init_basic_type(&string_type);
    
    any_type.kind_code = TSF_TK_ANY;
    any_type.hash_code = TSF_TK_ANY;
    init_basic_type(&any_type);
    any_type.is_dynamic = tsf_true;
}

static tsf_once_t once_control = TSF_ONCE_INIT;

static void check_init_types(void) {
    tsf_once(&once_control, init_types);
}

#define BIG_META_CODE_SWITCH(kind_code) \
    switch (kind_code) {                \
    case TSF_TK_VOID:                   \
        return &void_type;              \
    case TSF_TK_INT8:                   \
        return &int8_type;              \
    case TSF_TK_UINT8:                  \
        return &uint8_type;             \
    case TSF_TK_INT16:                  \
        return &int16_type;             \
    case TSF_TK_UINT16:                 \
        return &uint16_type;            \
    case TSF_TK_INT32:                  \
        return &int32_type;             \
    case TSF_TK_UINT32:                 \
        return &uint32_type;            \
    case TSF_TK_INT64:                  \
        return &int64_type;             \
    case TSF_TK_UINT64:                 \
        return &uint64_type;            \
    case TSF_TK_INTEGER:                \
        return &integer_type;           \
    case TSF_TK_LONG:                   \
        return &long_type;              \
    case TSF_TK_FLOAT:                  \
        return &float_type;             \
    case TSF_TK_DOUBLE:                 \
        return &double_type;            \
    case TSF_TK_BIT:                    \
        return &bit_type;               \
    case TSF_TK_STRING:                 \
        return &string_type;            \
    case TSF_TK_ANY:                    \
        return &any_type;               \
    default:                            \
        break;                          \
    }

static inline tsf_bool_t is_singleton(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:
    case TSF_TK_UINT8:
    case TSF_TK_INT16:
    case TSF_TK_UINT16:
    case TSF_TK_INT32:
    case TSF_TK_UINT32:
    case TSF_TK_INT64:
    case TSF_TK_UINT64:
    case TSF_TK_INTEGER:
    case TSF_TK_LONG:
    case TSF_TK_FLOAT:
    case TSF_TK_DOUBLE:
    case TSF_TK_BIT:
    case TSF_TK_VOID:
    case TSF_TK_STRING:
    case TSF_TK_ANY:
        return tsf_true;
    default:
        return tsf_false;
    }
}

tsf_type_t *tsf_type_create(tsf_type_kind_t kind_code) {
    tsf_type_t *ret;
    
    check_init_types();
    
    if (kind_code == TSF_TK_ARRAY) {
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "You cannot use TSF_TK_ARRAY with tsf_type_create(); "
                      "to create an array type, use tsf_type_create_array() "
                      "instead");
        return NULL;
    }
    
    BIG_META_CODE_SWITCH(kind_code)
    
    ret = malloc(sizeof(tsf_type_t));
    if (ret == NULL) {
        tsf_set_errno("Could not malloc tsf_type_t object.");
        return NULL;
    }

    tsf_atomic_count_init(&ret->ref_count, 1);
    
    ret->comment = NULL;
    ret->name = NULL;
    ret->version = 0;
    ret->kind_code = kind_code;
    ret->is_dynamic = tsf_false;
    switch (kind_code) {
    case TSF_TK_STRUCT:
        ret->u.s.tt = tsf_type_table_create();
        if (ret->u.s.tt == NULL) {
            free(ret);
            return NULL;
        }
        ret->u.s.has_size = tsf_false;
        ret->u.s.constructor = NULL;
        ret->u.s.pre_destructor = NULL;
        ret->u.s.destructor = NULL;
        break;
    case TSF_TK_CHOICE:
        ret->u.h.tt = tsf_type_table_create();
        if (ret->u.h.tt == NULL) {
            free(ret);
            return NULL;
        }
        ret->u.h.has_mapping = tsf_false;
        ret->u.h.choice_as_full_word = tsf_false;
        break;
    default:
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "'" fui8 "' is not a valid kind type for "
                      "tsf_type_create()",
                      kind_code);
        free(ret);
        return NULL;
    }
    
    tsf_type_recompute_hash(ret);
    
    ret->byte_size = 0;
    ret->memo_master = NULL;
    
    increment_type_balance();

    /* printf("created a type at %p\n",ret); */
    
    return ret;
}

tsf_type_t *tsf_type_create_array(tsf_type_t *ele_type) {
    tsf_type_t *ret;
    
    if (ele_type == NULL) {
        return NULL;
    }
    
    ret = malloc(sizeof(tsf_type_t));
    if (ret == NULL) {
        tsf_set_errno("Could not malloc tsf_type_t");
        tsf_type_destroy(ele_type);
        return NULL;
    }

    tsf_atomic_count_init(&ret->ref_count, 1);
    
    ret->comment = NULL;
    ret->name = NULL;
    ret->version = 0;
    ret->kind_code = TSF_TK_ARRAY;
    ret->is_dynamic = ele_type->is_dynamic;
    ret->u.a.element_type = ele_type;
    
    tsf_type_recompute_hash(ret);
    
    ret->byte_size = 0;
    ret->memo_master = NULL;
    
    increment_type_balance();

    /* printf("created an array type at %p\n",ret); */
    
    return ret;
}

typedef tsf_bool_t (*append_func_t)(tsf_type_t **type,
                                    const char *name,
                                    tsf_type_t *ele_type);

static tsf_type_t *create_somethingv(tsf_type_kind_t kind_code,
                                     append_func_t append,
                                     const char *first_name,
                                     va_list lst) {
    tsf_type_t *result;
    const char *name;
    tsf_type_t *type;
    
    result=tsf_type_create(kind_code);
    if (result==NULL) {
        return NULL;
    }
    
    name=first_name;
    
    while (name!=NULL) {
        type=va_arg(lst,tsf_type_t*);
        
        if (!append(&result,
                    name,
                    type)) {
            while (va_arg(lst,const char*)!=NULL) {
                tsf_type_destroy(va_arg(lst,tsf_type_t*));
            }
            tsf_type_destroy(result);
            return NULL;
        }
        
        name=va_arg(lst,const char*);
    }
    
    return result;
}

tsf_type_t *tsf_type_create_structv(const char *first_name,
                                    va_list lst) {
    return create_somethingv(TSF_TK_STRUCT,
                             tsf_struct_type_append,
                             first_name,
                             lst);
}

tsf_type_t *tsf_type_create_struct(const char *first_name,
                                   ...) {
    va_list lst;
    tsf_type_t *result;
    
    va_start(lst,first_name);
    
    result=tsf_type_create_structv(first_name,lst);
    
    va_end(lst);
    
    return result;
}

tsf_type_t *tsf_type_create_choicev(const char *first_name,
                                    va_list lst) {
    return create_somethingv(TSF_TK_CHOICE,
                             tsf_choice_type_append,
                             first_name,
                             lst);
}

tsf_type_t *tsf_type_create_choice(const char *first_name,
                                   ...) {
    va_list lst;
    tsf_type_t *result;
    
    va_start(lst,first_name);
    
    result=tsf_type_create_choicev(first_name,lst);
    
    va_end(lst);
    
    return result;
}

static tsf_bool_t type_kind_verify(tsf_type_kind_t code,
                                   tsf_limits_t *limits,
                                   tsf_error_t error) {
    if (tsf_limits_is_type_kind_allowed(limits,code)) {
        return tsf_true;
    }
    
    tsf_set_error(error,
                  "%s type is not allowed",
                  tsf_type_kind_tsf_name(code));
    return tsf_false;
}

tsf_type_t *tsf_type_read_rec(tsf_reader_t reader,
                              void *arg,
                              tsf_limits_t *limits,
                              uint32_t depth) {
    tsf_type_t *ret;
    uint8_t kind_code;
    uint8_t comment_mask;
    
    if (depth==tsf_limits_depth_limit(limits)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Type depth limit exceeded while parsing");
        return NULL;
    }
    
    if (!reader(arg,&kind_code,1)) {
        return NULL;
    }
    
    if (!type_kind_verify(kind_code,limits,TSF_E_PARSE_ERROR)) {
        return NULL;
    }
    
    BIG_META_CODE_SWITCH(kind_code)

    ret = malloc(sizeof(tsf_type_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_type_t object.");
        return NULL;
    }

    tsf_atomic_count_init(&ret->ref_count, 1);
    
    ret->comment = NULL;
    ret->name = NULL;
    ret->version = 0;
    ret->kind_code = kind_code;
    ret->memo_master = NULL;

    switch (ret->kind_code) {
    case TSF_TK_ARRAY:
        ret->u.a.element_type =
            tsf_type_read_rec(reader, arg, limits, depth + 1);
        if (ret->u.a.element_type == NULL) {
            free(ret);
            return NULL;
        }
        ret->is_dynamic = ret->u.a.element_type->is_dynamic;
        break;
    case TSF_TK_STRUCT:
        ret->u.s.tt = tsf_type_table_read("Struct",
                                          reader,
                                          arg,
                                          limits,
                                          tsf_limits_max_struct_size(limits),
                                          depth + 1);
        if (ret->u.s.tt == NULL) {
            free(ret);
            return NULL;
        }
        ret->is_dynamic = tsf_type_table_contains_dynamic(ret->u.s.tt);
        ret->u.s.has_size = tsf_false;
        ret->u.s.constructor = NULL;
        ret->u.s.pre_destructor = NULL;
        ret->u.s.destructor = NULL;
        break;
    case TSF_TK_CHOICE:
        ret->u.h.tt = tsf_type_table_read("Choice",
                                          reader,
                                          arg,
                                          limits,
                                          tsf_limits_max_choice_size(limits),
                                          depth + 1);
        if (ret->u.h.tt == NULL) {
            free(ret);
            return NULL;
        }
        ret->is_dynamic = tsf_type_table_contains_dynamic(ret->u.h.tt);
        ret->u.h.has_mapping = tsf_false;
        if (tsf_choice_type_get_num_elements(ret) >= 256)
            ret->u.h.choice_as_full_word = tsf_true;
        else
            ret->u.h.choice_as_full_word = tsf_false;
        break;
    default:
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "'" fui8 "' is not a valid kind type in "
                      "tsf_type_read()",
                      ret->kind_code);
        free(ret);
        return NULL;
    }
    
    increment_type_balance();
    
    if (!reader(arg, &comment_mask, 1)) {
        tsf_type_destroy(ret);
        return NULL;
    }
    
    if ((comment_mask & 1)&&
        !tsf_limits_allow_comments(limits)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Encountered a commented type while parsing, but "
                      "comments are not allowed");
        tsf_type_destroy(ret);
        return NULL;
    }
    
    if ((comment_mask & 1)) {
        ret->comment = tsf_read_string(reader, arg, TSF_MAX_COMMENT_SIZE);
        if (ret->comment == NULL) {
            tsf_type_destroy(ret);
            return NULL;
        }
    }
    
    if ((comment_mask & 2)) {
        ret->name = tsf_read_string(reader, arg, TSF_MAX_NAME_SIZE);
        if (ret->name == NULL) {
            tsf_type_destroy(ret);
            return NULL;
        }
    }
    
    if ((comment_mask & 4)) {
        if (!reader(arg, &ret->version, sizeof(ret->version))) {
            tsf_type_destroy(ret);
            return NULL;
        }
        ret->version = ntohl(ret->version);
    }
    
    if ((comment_mask & 8)) {
        if (ret->kind_code == TSF_TK_CHOICE) {
            ret->u.h.choice_as_full_word = tsf_false;
        }
    }
    
    tsf_type_recompute_hash(ret);
    
    ret->byte_size = 0;

    /* printf("read a type at %p\n",ret); */

    return ret;
}

tsf_type_t *tsf_type_read(tsf_reader_t reader,
                          void *arg,
                          tsf_limits_t *limits) {
    tsf_size_aware_rdr_t size_aware;
    tsf_type_t *result;
    
    check_init_types();
    
    size_aware.reader=reader;
    size_aware.arg=arg;
    size_aware.limit=tsf_limits_max_type_size(limits);
    size_aware.name="type";
    
    result=tsf_type_read_rec(tsf_size_aware_reader,
                             &size_aware,
                             limits,
                             0);
    
    if (result!=NULL) {
        result->byte_size=
            tsf_limits_max_type_size(limits)
            -size_aware.limit;
    }
    
    return result;
}

tsf_bool_t tsf_type_write(tsf_type_t *type,
                          tsf_writer_t writer,
                          void *arg) {
    uint8_t comment_mask;

    tsf_assert(tsf_atomic_count_value(&type->ref_count) > 0);

    if (!writer(arg,&type->kind_code,1)) {
        return tsf_false;
    }
    
    switch (type->kind_code) {
        case TSF_TK_ARRAY:
            if (!tsf_type_write(type->u.a.element_type,writer,arg)) {
                return tsf_false;
            }
            break;
        case TSF_TK_STRUCT:
            if (!tsf_type_table_write(type->u.s.tt,
                                      writer,
                                      arg)) {
                return tsf_false;
            }
            break;
        case TSF_TK_CHOICE:
            if (!tsf_type_table_write(type->u.h.tt,
                                      writer,
                                      arg)) {
                return tsf_false;
            }
            break;
        default:
            break;
    }
    
    if (tsf_type_kind_is_commentable(type->kind_code)) {
        comment_mask = 0;
        if (type->comment != NULL) {
            comment_mask |= 1;
        }
        if (type->name != NULL) {
            comment_mask |= 2;
        }
        if (type->version != 0) {
            comment_mask |= 4;
        }
        if (type->kind_code == TSF_TK_CHOICE
            && tsf_choice_type_get_num_elements(type) >= 256) {
            comment_mask |= 8;
        }
        
        if (!writer(arg, &comment_mask, 1)) {
            return tsf_false;
        }
        
        if (type->comment != NULL) {
            if (!tsf_write_string(writer, arg, type->comment)) {
                return tsf_false;
            }
        }
        
        if (type->name != NULL) {
            if (!tsf_write_string(writer, arg, type->name)) {
                return tsf_false;
            }
        }
        
        if (type->version != 0) {
            uint32_t version = htonl(type->version);
            if (!writer(arg, &version, sizeof(version))) {
                return tsf_false;
            }
        }
    }
    
    return tsf_true;
}

tsf_type_t *tsf_type_dup(tsf_type_t *type) {
    if (type == NULL) {
        return NULL;
    }
    
    BIG_META_CODE_SWITCH(type->kind_code)
        
    tsf_atomic_count_xadd(&type->ref_count, 1);
    
    increment_type_balance();

    /* printf("dupped a type at %p\n",type); */
    
    return type;
}

tsf_type_t *tsf_type_clone(tsf_type_t *type) {
    tsf_type_t *ret;
    
    if (type==NULL) {
        return NULL;
    }
    
    BIG_META_CODE_SWITCH(type->kind_code)

    if (type->kind_code==TSF_TK_ARRAY) {
        ret=tsf_type_create_array(tsf_type_clone(type->u.a.element_type));
    } else {
        ret=tsf_type_create(type->kind_code);
    }
    
    if (ret==NULL) {
        return NULL;
    }
    
    if (tsf_type_get_comment(type)!=NULL) {
        ret->comment=strdup(tsf_type_get_comment(type));
        if (ret->comment==NULL) {
            tsf_set_errno("Could not allocate comment");
            tsf_type_destroy(ret);
            return NULL;
        }
    }
    
    if (tsf_type_get_name(type)!=NULL) {
        ret->name=strdup(tsf_type_get_name(type));
        if (ret->name==NULL) {
            tsf_set_errno("Could not allocate name");
            tsf_type_destroy(ret);
            return NULL;
        }
    }
    
    ret->version=type->version;
    
    if (type->kind_code==TSF_TK_STRUCT) {
        if (!tsf_type_table_copy(ret->u.s.tt,type->u.s.tt)) {
            tsf_type_destroy(ret);
            return NULL;
        }
        ret->u.s.has_size=type->u.s.has_size;
        ret->u.s.size=type->u.s.size;
    } else if (type->kind_code==TSF_TK_CHOICE) {
        if (!tsf_type_table_copy(ret->u.h.tt,type->u.h.tt)) {
            tsf_type_destroy(ret);
            return NULL;
        }
        ret->u.h.has_mapping=type->u.h.has_mapping;
        ret->u.h.in_place=type->u.h.in_place;
        ret->u.h.data_offset=type->u.h.data_offset;
        ret->u.h.value_offset=type->u.h.value_offset;
        ret->u.h.size=type->u.h.size;
    }
    
    ret->is_dynamic=type->is_dynamic;
    tsf_type_recompute_hash(ret);
    ret->byte_size=type->byte_size;
    
    return ret;
}

tsf_type_t *tsf_type_own_begin(tsf_type_t *type) {
    if (tsf_atomic_count_value(&type->ref_count) == 1) {
        return type;
    }
    
    return tsf_type_clone(type);
}

void tsf_type_own_commit(tsf_type_t **dst, tsf_type_t *ret) {
    if (*dst == ret) {
        return;
    }
    
    tsf_type_destroy(*dst);
    *dst = ret;
}

void tsf_type_own_abort(tsf_type_t *dst, tsf_type_t *ret) {
    if (dst == ret) {
        return;
    }
    
    tsf_type_destroy(ret);
}

void tsf_type_destroy(tsf_type_t *type) {
    /* printf("tsf_type: destroy: %p\n", type); */
    
    if (is_singleton(type->kind_code))
        return;
    
    if (tsf_atomic_count_xsub(&type->ref_count, 1) > 1) {
        /* printf("didn't destroy a type at %p\n",type); */
        return;     
    }
    
    /* printf("tsf_type: really destroy: %p\n", type); */
    
    if (type->memo_master != NULL) {
        if (type->memo_master == type) {
            /* need to remove myself from the memo table. */
            tsf_type_t *to_delete = type;
            tsf_mutex_lock(&memo_lock);
            tsf_st_delete(memo_table, (char**)&to_delete, NULL);
            tsf_mutex_unlock(&memo_lock);
        } else {
            /* relinquish ownership of the memo master. */
            /* printf("Destroying memo master: %p\n", type->memo_master); */
            tsf_type_destroy(type->memo_master);
        }
    }
    
    switch (type->kind_code) {
        case TSF_TK_ARRAY:
            tsf_type_destroy(type->u.a.element_type);
            break;
        case TSF_TK_STRUCT:
            tsf_type_table_destroy(type->u.s.tt);
            break;
        case TSF_TK_CHOICE:
            tsf_type_table_destroy(type->u.h.tt);
        default:
            break;
    }

    tsf_atomic_count_destroy(&type->ref_count);
    
    if (type->comment!=NULL) {
        free(type->comment);
    }
    
    if (type->name!=NULL) {
        free(type->name);
    }
    
    free(type);
    
    decrement_type_balance();

    /* printf("destroyed a type at %p\n",type); */
}

static tsf_bool_t type_verify_rec(tsf_type_t *type,
                                  tsf_limits_t *limits,
                                  uint32_t depth) {
    uint32_t i;

    if (depth==tsf_limits_depth_limit(limits)) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Type exceeds depth limit");
        return tsf_false;
    }
    
    if (!type_kind_verify(type->kind_code,limits,TSF_E_NOT_ALLOWED)) {
        return tsf_false;
    }
    
    if (type->comment!=NULL &&
        !tsf_limits_allow_comments(limits)) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Encountered comments, but commented are not allowed");
        return tsf_false;
    }
    
    switch (type->kind_code) {
        case TSF_TK_ARRAY:
            if (!type_verify_rec(type->u.a.element_type,
                                 limits,
                                 depth+1)) {
                return tsf_false;
            }
            break;
        case TSF_TK_STRUCT:
            if (tsf_struct_type_get_num_elements(type)>
                tsf_limits_max_struct_size(limits)) {
                tsf_set_error(TSF_E_NOT_ALLOWED,
                              "Structure type size limit exceeded");
                return tsf_false;
            }
            for (i=0;
                 i<tsf_struct_type_get_num_elements(type);
                 ++i) {
                if (tsf_struct_type_get_element(type,i)->comment!=NULL &&
                    !tsf_limits_allow_comments(limits)) {
                    tsf_set_error(TSF_E_NOT_ALLOWED,
                                  "Encountered comments, but commented are "
                                  "not allowed");
                    return tsf_false;
                }
                
                if (!type_verify_rec(tsf_struct_type_get_element(type,i)->type,
                                     limits,
                                     depth+1)) {
                    return tsf_false;
                }
            }
            break;
        case TSF_TK_CHOICE:
            if (tsf_choice_type_get_num_elements(type)>
                tsf_limits_max_choice_size(limits)) {
                tsf_set_error(TSF_E_NOT_ALLOWED,
                              "Choice type size limit exceeded");
                return tsf_false;
            }
            for (i=0;
                 i<tsf_choice_type_get_num_elements(type);
                 ++i) {
                if (!type_verify_rec(tsf_choice_type_get_element(type,i)->type,
                                     limits,
                                     depth+1)) {
                    return tsf_false;
                }
            }
            break;
        default:
            break;
    }
    
    return tsf_true;
}

uint32_t tsf_type_get_byte_size(tsf_type_t *type) {
    uint32_t size=type->byte_size;
    if (size==0) {
        tsf_bool_t result=tsf_type_write(type,
                                         tsf_size_calc_writer,
                                         &size);
        tsf_assert(result);
        type->byte_size=size;
    }
    return size;
}

tsf_bool_t tsf_type_verify(tsf_type_t *type,
                           tsf_limits_t *limits) {
    if (limits==NULL) {
        return tsf_true;
    }
    
    if (tsf_type_get_byte_size(type)>
        tsf_limits_max_type_size(limits)) {
        tsf_set_error(TSF_E_NOT_ALLOWED,
                      "Type is larger than type size limit");
        return tsf_false;
    }
    
    /* verify all else */
    return type_verify_rec(type,limits,0);
}

int tsf_type_get_hash(tsf_type_t *type) {
    return type->hash_code;
}

void tsf_type_recompute_hash(tsf_type_t *type) {
    type->hash_code = type->kind_code;
    switch (type->kind_code) {
    case TSF_TK_ARRAY:
        type->hash_code += tsf_type_get_hash(type->u.a.element_type);
        break;
    case TSF_TK_STRUCT:
        type->hash_code += tsf_type_table_get_hash(type->u.s.tt);
        break;
    case TSF_TK_CHOICE:
        type->hash_code +=
            tsf_type_table_get_hash(type->u.h.tt) +
            type->u.h.choice_as_full_word;
        break;
    default:
        break;
    }
}

tsf_bool_t tsf_type_compare(tsf_type_t *a,
                            tsf_type_t *b) {
    if (a == b) {
        return tsf_true;
    }
    
    if (a->kind_code != b->kind_code) {
        return tsf_false;
    }
    
    switch (a->kind_code) {
    case TSF_TK_ARRAY:
        if (!tsf_type_compare(a->u.a.element_type,
                              b->u.a.element_type)) {
            return tsf_false;
        }
        break;
    case TSF_TK_STRUCT:
        if (!tsf_type_table_compare(a->u.s.tt,
                                    b->u.s.tt)) {
            return tsf_false;
        }
        break;
    case TSF_TK_CHOICE:
        if (!tsf_type_table_compare(a->u.h.tt,
                                    b->u.h.tt)) {
            return tsf_false;
        }
        if (a->u.h.choice_as_full_word != b->u.h.choice_as_full_word) {
            return tsf_false;
        }
        break;
    default:
        break;
    }
    
    return tsf_true;
}

tsf_type_t *tsf_type_memo(tsf_type_t *type) {
    tsf_type_t *result;
    
    result = type->memo_master;
    if (result) {
        return result;
    }
    
    tsf_mutex_lock(&memo_lock);
    if (tsf_st_lookup(memo_table, (char*)type, (void**)&result)) {
        type->memo_master = tsf_type_dup(result);
    } else {
        if (tsf_st_add_direct(memo_table, (char*)type, (void*)type) < 0) {
            tsf_set_errno("Could not memoize type");
            result = NULL;
        } else {
            type->memo_master = type;
            result = type;
        }
    }
    tsf_mutex_unlock(&memo_lock);
    
    return result;
}

tsf_bool_t tsf_type_is_dynamic(tsf_type_t *type) {
    return type->is_dynamic;
}

tsf_bool_t tsf_type_instanceof(tsf_type_t *one,
                               tsf_type_t *two) {
    if (two->kind_code==TSF_TK_VOID) {
        return tsf_true;
    }

    if (tsf_type_kind_is_primitive(one->kind_code) ||
	one->kind_code==TSF_TK_BIT) {
        return tsf_type_kind_is_primitive(two->kind_code)
	    || two->kind_code==TSF_TK_BIT;
    }
    
    if (one->kind_code!=two->kind_code) {
        return tsf_false;
    }

    switch (one->kind_code) {
        case TSF_TK_ARRAY:
            return tsf_type_instanceof(one->u.a.element_type,
                                       two->u.a.element_type);
        case TSF_TK_STRUCT:
            {
                uint32_t i;
                for (i=0;i<tsf_struct_type_get_num_elements(two);++i) {
                    tsf_named_type_t *n=tsf_struct_type_get_element(two,i);
                    tsf_named_type_t *n2=
                        tsf_struct_type_find_node(one,
                            tsf_named_type_get_name(n));
		    if (n->optional) {
			continue;
		    }
                    if (n2==NULL || n2->optional) {
                        return tsf_false;
                    }
                    if (!tsf_type_instanceof(
                             tsf_named_type_get_type(n2),
                             tsf_named_type_get_type(n))) {
                        return tsf_false;
                    }
                }
            }
            break;
        case TSF_TK_CHOICE:
            {
                uint32_t i;
                for (i=0;i<tsf_choice_type_get_num_elements(one);++i) {
                    tsf_named_type_t *n=tsf_choice_type_get_element(one,i);
                    tsf_named_type_t *n2=
                        tsf_choice_type_find_node(two,
                            tsf_named_type_get_name(n));
                    
                    if (n2==NULL) {
                        continue;
                    }
                    
                    if (!tsf_type_instanceof(
                             tsf_named_type_get_type(n),
                             tsf_named_type_get_type(n2))) {
                        return tsf_false;
                    }
                }
            }
        default:
            break;
    }
    
    return tsf_true;
}

tsf_type_kind_t tsf_type_get_kind_code(tsf_type_t *type) {
    return type->kind_code;
}

uint32_t tsf_type_get_static_size(tsf_type_t *type) {
    switch (type->kind_code) {
    case TSF_TK_ARRAY:
        return UINT32_MAX;
    case TSF_TK_STRUCT: {
        uint32_t i, num_bits = 0, ret = 0;
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            uint32_t tmp;
            if (tsf_struct_type_get_element(type,i)->type->kind_code == TSF_TK_BIT) {
                ++num_bits;
                continue;
            }
            tmp = tsf_type_get_static_size(
                tsf_struct_type_get_element(type, i)->type);
            if (tmp == UINT32_MAX) {
                return UINT32_MAX;
            }
            ret += tmp;
        }
        ret += ((num_bits + 7) >> 3);
        return ret;
    }
    case TSF_TK_CHOICE:
        if (tsf_choice_type_has_non_void(type)) {
            return UINT32_MAX;
        } else {
            if (tsf_choice_type_get_num_elements(type) >= 256) {
                if (type->u.h.choice_as_full_word) {
                    return 4;
                } else {
                    return UINT32_MAX;
                }
            } else {
                return 1;
            }
        }
        break;
    case TSF_TK_BIT:
        return 1;
    case TSF_TK_VOID:
        return 0;
    case TSF_TK_STRING:
        return UINT32_MAX;
    case TSF_TK_ANY:
        return UINT32_MAX;
    case TSF_TK_INTEGER:
    case TSF_TK_LONG:
        return UINT32_MAX;
    default:
        return tsf_primitive_type_kind_size_of(type->kind_code);
    }
}

const char *tsf_type_get_comment(tsf_type_t *type) {
    return type->comment;
}

tsf_bool_t tsf_type_has_comment(tsf_type_t *type) {
    return type->comment != NULL;
}

tsf_bool_t tsf_type_set_comment(tsf_type_t **type,
                                const char *comment) {
    char *comment_copy;
    tsf_type_t *ret;
    
    if (!tsf_type_kind_is_commentable((*type)->kind_code)) {
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "The given type is not commentable");
        return tsf_false;
    }
    
    if (strlen(comment) + 1 > TSF_MAX_COMMENT_SIZE) {
        tsf_set_error(TSF_E_STR_OVERFLOW,
                      "String '%s' is too long to be used as a type comment",
                      comment);
        return tsf_false;
    }
    
    comment_copy = strdup(comment);
    if (comment_copy == NULL) {
        tsf_set_errno("Could not stdup comment");
        return tsf_false;
    }
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        free(comment_copy);
        return tsf_false;
    }
    
    if (ret->comment != NULL) {
        free(ret->comment);
    }
    ret->comment = comment_copy;
    ret->byte_size = 0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

const char *tsf_type_get_name(tsf_type_t *type) {
    return type->name;
}

tsf_bool_t tsf_type_has_name(tsf_type_t *type) {
    return type->name != NULL;
}

tsf_bool_t tsf_type_set_name(tsf_type_t **type,
                             const char *name) {
    char *name_copy;
    tsf_type_t *ret;
    
    if (!tsf_type_kind_is_commentable((*type)->kind_code)) {
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "The given type is not commentable");
        return tsf_false;
    }
    
    if (strlen(name) + 1 > TSF_MAX_NAME_SIZE) {
        tsf_set_error(TSF_E_STR_OVERFLOW,
                      "String '%s' is too long to be used as a type name",
                      name);
        return tsf_false;
    }
    
    name_copy = strdup(name);
    if (name_copy == NULL) {
        tsf_set_errno("Could not strdup name");
        return tsf_false;
    }
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        free(name_copy);
        return tsf_false;
    }
    
    if (ret->name != NULL) {
        free(ret->name);
    }
    ret->name = name_copy;
    ret->byte_size = 0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_type_has_version(tsf_type_t *type) {
    return type->version != 0;
}

uint32_t tsf_type_get_raw_version(tsf_type_t *type) {
    return type->version;
}

uint16_t tsf_type_get_major_version(tsf_type_t *type) {
    return (type->version >> 16) & 0xffff;
}

uint8_t tsf_type_get_minor_version(tsf_type_t *type) {
    return (type->version >> 8) & 0xff;
}

uint8_t tsf_type_get_patch_version(tsf_type_t *type) {
    return (type->version >> 0) & 0xff;
}

tsf_bool_t tsf_type_set_raw_version(tsf_type_t **type,
                                    uint32_t version) {
    tsf_type_t *ret;
    
    if (!tsf_type_kind_is_commentable((*type)->kind_code)) {
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "The given type is not commentable");
        return tsf_false;
    }
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    ret->version = version;
    ret->byte_size = 0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_type_set_version(tsf_type_t **type,
                                uint16_t major,
                                uint8_t minor,
                                uint8_t patch) {
    return tsf_type_set_raw_version(type,
                                    ((((uint32_t)major) & 0xffff) << 16) |
                                    ((((uint32_t)minor) & 0xff) << 8) |
                                    ((((uint32_t)patch) & 0xff) << 0));
}

tsf_type_t *tsf_array_type_get_element_type(tsf_type_t *type) {
    return type->u.a.element_type;
}

tsf_bool_t tsf_struct_type_append(tsf_type_t **type,
                                  const char *name,
                                  tsf_type_t *ele_type) {
    tsf_type_t *ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        if (ele_type!=NULL) {
            tsf_type_destroy(ele_type);
        }
        return tsf_false;
    }
    
    if (!tsf_type_table_append(ret->u.s.tt,name,ele_type)) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    if (ele_type->is_dynamic) {
        ret->is_dynamic=tsf_true;
    }
    
    /* depending on the invariant that the hash code of the type table is
     * just the sum of the hash codes of the named types. */
    ret->hash_code +=
        tsf_named_type_get_hash(
            tsf_type_table_find_by_index(ret->u.s.tt,
                tsf_type_table_get_num_elements(ret->u.s.tt)-1));

    ret->byte_size=0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_struct_type_remove(tsf_type_t **type,
                                  const char *name) {
    tsf_type_t *ret;
    tsf_named_type_t *named_type;
    int hash_code;
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    named_type = tsf_type_table_find_by_name(ret->u.s.tt, name);
    if (named_type == NULL) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    /* depending on the invariant that the hash code of the type table
     * is just the sum of the hash codes of the named types. */
    hash_code = tsf_named_type_get_hash(named_type);

    if (!tsf_type_table_remove(ret->u.s.tt, name)) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    ret->hash_code -= hash_code;
    ret->byte_size = 0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_struct_type_set_comment(tsf_type_t **type,
                                       const char *name,
                                       const char *comment) {
    tsf_type_t *ret;
    tsf_named_type_t *n;
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    n = tsf_struct_type_find_node(ret,name);
    
    if (n == NULL) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    if (!tsf_named_type_set_comment(n, comment)) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    ret->byte_size = 0;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_struct_type_set_optional(tsf_type_t **type,
					const char *name,
					tsf_bool_t optional) {
    tsf_type_t *ret;
    tsf_named_type_t *n;
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    n = tsf_struct_type_find_node(ret, name);
    
    if (n == NULL) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    tsf_named_type_set_optional(n, optional);
    
    ret->byte_size = 0;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_type_t *tsf_struct_type_find(tsf_type_t *type, const char *name) {
    tsf_named_type_t *n = tsf_struct_type_find_node(type, name);
    if (n == NULL) {
        return NULL;
    }
    
    return n->type;
}

tsf_named_type_t *tsf_struct_type_find_node(tsf_type_t *type,
                                            const char *name) {
    return tsf_type_table_find_by_name(type->u.s.tt, name);
}

uint32_t tsf_struct_type_get_num_elements(tsf_type_t *type) {
    return tsf_type_table_get_num_elements(type->u.s.tt);
}

tsf_named_type_t *tsf_struct_type_get_element(tsf_type_t *type,
                                              uint32_t index) {
    return tsf_type_table_find_by_index(type->u.s.tt, index);
}

const char *tsf_struct_type_get_element_name(tsf_type_t *type,
                                             uint32_t index) {
    return tsf_named_type_get_name(
        tsf_struct_type_get_element(type, index));
}

tsf_type_t *tsf_struct_type_get_element_type(tsf_type_t *type,
                                             uint32_t index) {
    return tsf_named_type_get_type(
        tsf_struct_type_get_element(type, index));
}

tsf_bool_t tsf_choice_type_append(tsf_type_t **type,
                                  const char *name,
                                  tsf_type_t *ele_type) {
    tsf_type_t *ret;
    
    if (!strcmp(name, "unknown")) {
        tsf_set_error(TSF_E_NAME_COLLISION,
                      "The name 'unknown' is reserved in choice types");
        if (ele_type != NULL) {
            tsf_type_destroy(ele_type);
        }
        return tsf_false;
    }
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        if (ele_type != NULL) {
            tsf_type_destroy(ele_type);
        }
        return tsf_false;
    }

    if (!tsf_type_table_append(ret->u.h.tt, name, ele_type)) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    if (ele_type->is_dynamic) {
        ret->is_dynamic = tsf_true;
    }
    
    /* depending on the invariant that the hash code of the type table is
     * just the sum of the hash codes of the named types. */
    ret->hash_code +=
        tsf_named_type_get_hash(
            tsf_type_table_find_by_index(ret->u.h.tt,
                tsf_type_table_get_num_elements(ret->u.h.tt) - 1));

    ret->byte_size = 0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_choice_type_remove(tsf_type_t **type,
                                  const char *name) {
    tsf_type_t *ret;
    tsf_named_type_t *named_type;
    int hash_code;
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    named_type = tsf_type_table_find_by_name(ret->u.h.tt, name);
    if (named_type == NULL) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    /* depending on the invariant that the hash code of the type table
     * is just the sum of the hash codes of the named types. */
    hash_code = tsf_named_type_get_hash(named_type);

    if (!tsf_type_table_remove(ret->u.h.tt, name)) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    ret->hash_code -= hash_code;
    ret->byte_size = 0;
    
    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_bool_t tsf_choice_type_set_comment(tsf_type_t **type,
                                       const char *name,
                                       const char *comment) {
    tsf_type_t *ret;
    tsf_named_type_t *n;
    
    ret = tsf_type_own_begin(*type);
    if (ret == NULL) {
        return tsf_false;
    }
    
    n = tsf_choice_type_find_node(ret,name);
    
    if (n == NULL) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    if (!tsf_named_type_set_comment(n, comment)) {
        tsf_type_own_abort(*type, ret);
        return tsf_false;
    }
    
    ret->byte_size = 0;

    tsf_type_own_commit(type, ret);
    return tsf_true;
}

tsf_type_t *tsf_choice_type_find(tsf_type_t *type,const char *name) {
    tsf_named_type_t *n=tsf_choice_type_find_node(type,name);
    if (n==NULL) {
        return NULL;
    }
    
    return n->type;
}

tsf_named_type_t *tsf_choice_type_find_node(tsf_type_t *type,
                                            const char *name) {
    return tsf_type_table_find_by_name(type->u.h.tt, name);
}

uint32_t tsf_choice_type_get_num_elements(tsf_type_t *type) {
    return tsf_type_table_get_num_elements(type->u.h.tt);
}

tsf_named_type_t *tsf_choice_type_get_element(tsf_type_t *type,
                                              uint32_t index) {
    return tsf_type_table_find_by_index(type->u.h.tt, index);
}

const char *tsf_choice_type_get_element_name(tsf_type_t *type,
                                             uint32_t index) {
    return tsf_named_type_get_name(
        tsf_choice_type_get_element(type, index));
}

tsf_type_t *tsf_choice_type_get_element_type(tsf_type_t *type,
                                             uint32_t index) {
    return tsf_named_type_get_type(
        tsf_choice_type_get_element(type, index));
}

tsf_bool_t tsf_choice_type_has_non_void(tsf_type_t *type) {
    uint32_t i;
    for (i=0;i<tsf_type_table_get_num_elements(type->u.h.tt);++i) {
        if (tsf_type_table_find_by_index(type->u.h.tt,i)->
                type->kind_code!=TSF_TK_VOID) {
            return tsf_true;
        }
    }
    return tsf_false;
}

tsf_bool_t tsf_choice_type_uses_same_indices(tsf_type_t *a,
                                             tsf_type_t *b) {
    uint32_t i;
    if (tsf_choice_type_get_num_elements(a)>
        tsf_choice_type_get_num_elements(b)) {
        return tsf_false;
    }
    for (i=0;i<tsf_choice_type_get_num_elements(a);++i) {
        tsf_named_type_t *n=tsf_choice_type_get_element(a,i);
        tsf_named_type_t *n2=tsf_choice_type_get_element(b,i);
        if (strcmp(tsf_named_type_get_name(n),
                   tsf_named_type_get_name(n2))) {
            return tsf_false;
        }
    }
    return tsf_true;
}




