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

#include <unistd.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#ifdef DEBUG_REFLECT_ALLOC
static int64_t reflect_balance = 0;

int64_t tsf_reflect_get_balance() {
    return reflect_balance;
}

#define increment_reflect_balance() do {\
    reflect_balance++;\
} while(0)

#define decrement_reflect_balance() do {\
    reflect_balance--;\
} while(0)

#else

#define increment_reflect_balance() do {\
} while(0)

#define decrement_reflect_balance() do {\
} while(0)

#endif

#define is_struct_or_array(type) \
    (type->kind_code == TSF_TK_STRUCT || \
     type->kind_code == TSF_TK_ARRAY)

tsf_reflect_t *tsf_reflect_create(tsf_type_t *type) {
    tsf_reflect_t *ret;
    
    if (type == NULL) {
        return NULL;
    }
    
    ret = malloc(sizeof(tsf_reflect_t));
    if (ret == NULL) {
        tsf_set_errno("Could not allocate tsf_reflect_t structure.");
        return NULL;
    }
    
    ret->type = tsf_type_dup(type);
    
    switch (type->kind_code) {
    case TSF_TK_ARRAY:
        ret->u.c.array = NULL;
        ret->u.c.size = 0;
        break;
    case TSF_TK_STRUCT:
        ret->u.c.size = tsf_struct_type_get_num_elements(type);
        ret->u.c.array = malloc(sizeof(tsf_reflect_t*) *
                                ret->u.c.size);
        if (ret->u.c.array == NULL) {
            free(ret);
            tsf_type_destroy(ret->type);
            tsf_set_errno("Could not allocate array of pointers in "
                          "tsf_reflect_create()");
            return NULL;
        }
        bzero(ret->u.c.array, sizeof(tsf_reflect_t*) * ret->u.c.size);
        break;
    case TSF_TK_CHOICE:
        ret->u.h.choice = UINT32_MAX;
        ret->u.h.data = NULL;
        break;
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
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "To create tsf_reflect_t objects of type "
                      "%s, use tsf_reflect_create_%s()",
                      tsf_type_kind_tsf_name(type->kind_code),
                      tsf_type_kind_lc_name(type->kind_code));
        tsf_type_destroy(ret->type);
        free(ret);
        ret = NULL;
        break;
    default:
        tsf_type_destroy(ret->type);
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      "'" fui8 "' is not a valid kind type for "
                      "tsf_reflect_create()",
                      type->kind_code);
        free(ret);
        ret = NULL;
        break;
    }

    increment_reflect_balance();
    
    return ret;
}

#define DEFINE_REFLECT_CREATE_GET(type_lc_name, type_lc, type_uc)       \
    tsf_reflect_t *tsf_reflect_create_##type_lc_name(type_lc value) {   \
        tsf_reflect_t *ret;                                             \
                                                                        \
        ret = malloc(sizeof(tsf_reflect_t));                            \
        if (ret == NULL) {                                              \
            tsf_set_errno("Could not allocate tsf_reflect_t structure."); \
            return NULL;                                                \
        }                                                               \
                                                                        \
        ret->type = tsf_type_create(type_uc);                           \
        ret->u.p_##type_lc_name = value;                                \
                                                                        \
        increment_reflect_balance();                                    \
                                                                        \
        return ret;                                                     \
    }                                                                   \
                                                                        \
    type_lc tsf_##type_lc_name##_reflect_get(tsf_reflect_t *data) {     \
        return data->u.p_##type_lc_name;                                \
    }

DEFINE_REFLECT_CREATE_GET(int16_t, int16_t, TSF_TK_INT16)
DEFINE_REFLECT_CREATE_GET(uint16_t, uint16_t, TSF_TK_UINT16)
DEFINE_REFLECT_CREATE_GET(int32_t, int32_t, TSF_TK_INT32)
DEFINE_REFLECT_CREATE_GET(uint32_t, uint32_t, TSF_TK_UINT32)
DEFINE_REFLECT_CREATE_GET(int64_t, int64_t, TSF_TK_INT64)
DEFINE_REFLECT_CREATE_GET(uint64_t, uint64_t, TSF_TK_UINT64)
DEFINE_REFLECT_CREATE_GET(integer, int32_t, TSF_TK_INTEGER)
DEFINE_REFLECT_CREATE_GET(long, int64_t, TSF_TK_LONG)
DEFINE_REFLECT_CREATE_GET(float, float, TSF_TK_FLOAT)
DEFINE_REFLECT_CREATE_GET(double, double, TSF_TK_DOUBLE)

static tsf_reflect_t int8_data[256];
static tsf_reflect_t uint8_data[256];
static tsf_reflect_t bit_data_true,
                     bit_data_false;
static tsf_reflect_t void_data;

static void init_reflect(void) {
    unsigned i;
    
    for (i = 0; i < 256; ++i) {
        int8_data[i].type = tsf_type_create(TSF_TK_INT8);
        int8_data[i].u.p_int8_t = i - 128;
        
        uint8_data[i].type = tsf_type_create(TSF_TK_UINT8);
        uint8_data[i].u.p_uint8_t = i;
    }
    
    bit_data_true.type = tsf_type_create(TSF_TK_BIT);
    bit_data_true.u.bit = tsf_true;
    
    bit_data_false.type = tsf_type_create(TSF_TK_BIT);
    bit_data_false.u.bit = tsf_false;
    
    void_data.type = tsf_type_create(TSF_TK_VOID);
}

static tsf_once_t once_control = TSF_ONCE_INIT;
static void check_init_reflect(void) {
    tsf_once(&once_control, init_reflect);
}

tsf_reflect_t *tsf_reflect_create_int8_t(int8_t value) {
    check_init_reflect();
    return int8_data + value + 128;
}

tsf_reflect_t *tsf_reflect_create_uint8_t(uint8_t value) {
    check_init_reflect();
    return uint8_data + value;
}

int8_t tsf_int8_t_reflect_get(tsf_reflect_t *data) {
    return data->u.p_int8_t;
}

uint8_t tsf_uint8_t_reflect_get(tsf_reflect_t *data) {
    return data->u.p_uint8_t;
}

tsf_reflect_t *tsf_reflect_create_bit(tsf_bool_t value) {
    check_init_reflect();
    return value ? &bit_data_true : &bit_data_false;
}

tsf_reflect_t *tsf_reflect_create_void(void) {
    check_init_reflect();
    return &void_data;
}

tsf_reflect_t *tsf_reflect_create_string(const char *str) {
    tsf_reflect_t *ret = malloc(sizeof(tsf_reflect_t));
    if (ret == NULL) {
        tsf_set_errno("Could not malloc tsf_reflect_t");
        return NULL;
    }
    
    ret->type = tsf_type_create(TSF_TK_STRING);
    ret->u.str = strdup(str);
    
    if (ret->u.str == NULL) {
        free(ret);
        tsf_set_errno("Could not strdup()");
        return NULL;
    }
    
    increment_reflect_balance();
    
    return ret;
}

tsf_reflect_t *tsf_reflect_create_any(tsf_buffer_t *buf) {
    tsf_reflect_t *ret=malloc(sizeof(tsf_reflect_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_reflect_t");
        tsf_buffer_destroy(buf);
        return NULL;
    }
    
    ret->type=tsf_type_create(TSF_TK_ANY);
    ret->u.any=buf;
    
    increment_reflect_balance();
    
    return ret;
}

tsf_reflect_t *tsf_reflect_clone(tsf_reflect_t *data) {
    uint32_t i;
    tsf_reflect_t *ret;
    
    switch (data->type->kind_code) {
    case TSF_TK_UINT8:
    case TSF_TK_INT8:
    case TSF_TK_BIT:
    case TSF_TK_VOID:
        return data;
    default:
        break;
    }
    
    ret = malloc(sizeof(tsf_reflect_t));
    
    if (ret == NULL) {
        tsf_set_errno("Could not allocate tsf_reflect_t structure.");
        return NULL;
    }
    
    ret->type = tsf_type_dup(data->type);
    
    if (is_struct_or_array(data->type)) {
        ret->u.c.size = data->u.c.size;
        ret->u.c.array = malloc(sizeof(tsf_reflect_t*) * ret->u.c.size);
        if (ret->u.c.array == NULL) {
            tsf_set_errno("Could not allocate array of pointers in "
                          "tsf_reflect_clone()");
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
        
        /* set the elements of the array to NULL so that destroy
         * works. */
        bzero(ret->u.c.array,
              sizeof(tsf_reflect_t*) * ret->u.c.size);
        
        for (i = 0; i < ret->u.c.size; ++i) {
            if (data->u.c.array[i] == NULL) {
                continue;
            }
            
            ret->u.c.array[i] = tsf_reflect_clone(data->u.c.array[i]);
            if (ret->u.c.array[i] == NULL) {
                tsf_type_destroy(ret->type);
                tsf_reflect_destroy(ret);
                return NULL;
            }
        }
    } else if (data->type->kind_code == TSF_TK_CHOICE) {
        ret->u.h.choice = data->u.h.choice;
        ret->u.h.data = tsf_reflect_clone(data->u.h.data);
        if (ret->u.h.data == NULL) {
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
    } else if (data->type->kind_code == TSF_TK_STRING) {
        ret->u.str = strdup(data->u.str);
        if (ret->u.str == NULL) {
            tsf_set_errno("Could not strdup()");
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
    } else if (data->type->kind_code == TSF_TK_ANY) {
        ret->u.any = tsf_buffer_dup(data->u.any);
        if (ret->u.any == NULL) {
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
    } else {
        /* just copy the bits */
        ret->u = data->u;
    }
    
    increment_reflect_balance();
    
    return ret;
}

void tsf_reflect_destroy(tsf_reflect_t *data) {
    uint32_t i;
    
    switch (data->type->kind_code) {
    case TSF_TK_UINT8:
    case TSF_TK_INT8:
    case TSF_TK_BIT:
    case TSF_TK_VOID:
        return;
    default:
        break;
    }
    
    decrement_reflect_balance();
    
    if (is_struct_or_array(data->type)) {
        for (i = 0; i < data->u.c.size; ++i) {
            if (data->u.c.array[i] == NULL) {
                continue;
            }
            
            tsf_reflect_destroy(data->u.c.array[i]);
        }
        
        if (data->u.c.array != NULL) {
            free(data->u.c.array);
        }
    } else if (data->type->kind_code == TSF_TK_CHOICE) {
        if (data->u.h.data != NULL) {
            tsf_reflect_destroy(data->u.h.data);
        }
    } else if (data->type->kind_code == TSF_TK_STRING) {
        free(data->u.str);
    } else if (data->type->kind_code == TSF_TK_ANY) {
        tsf_buffer_destroy(data->u.any);
    }
    
    tsf_type_destroy(data->type);

    free(data);
}

tsf_bool_t tsf_reflect_verify(tsf_reflect_t *data) {
    uint32_t i;
    
    if (!is_struct_or_array(data->type)) {
        return tsf_true;
    }
    
    for (i = 0; i < data->u.c.size; ++i) {
        if (data->u.c.array[i] == NULL) {
            tsf_set_error(TSF_E_ELEMENT_NULL,
                          "In %s at index = " fui32,
                          data->type->kind_code == TSF_TK_ARRAY ?
                          "an array" : "a struct",
                          i);
            return tsf_false;
        }
    }
    
    return tsf_true;
}

/* does a prepass for producing a buffer */
static uint32_t write_calc_size(tsf_reflect_t *data) {
    uint32_t i, num_bits = 0, ret = 0;
    
    switch (data->type->kind_code) {
    case TSF_TK_INTEGER:
        return size_of_tsf_integer(data->u.p_integer);
    case TSF_TK_LONG:
        return size_of_tsf_long(data->u.p_long);
    default:
        break;
    }
    
    if (tsf_type_kind_is_primitive(data->type->kind_code)) {
        return tsf_primitive_type_kind_size_of(data->type->kind_code);
    }
    
    if (data->type->kind_code == TSF_TK_BIT) {
        return 1;
    }
    
    if (data->type->kind_code == TSF_TK_VOID) {
        return 0;
    }
    
    if (data->type->kind_code == TSF_TK_STRING) {
        return strlen(data->u.str) + 1;
    }
    
    if (data->type->kind_code == TSF_TK_ANY) {
        return tsf_buffer_calc_size(data->u.any);
    }
    
    if (data->type->kind_code == TSF_TK_CHOICE) {
        if (tsf_choice_type_get_num_elements(data->type) >= 256) {
            if (data->type->u.h.choice_as_full_word) {
                ret += 4;
            } else {
                ret += size_of_tsf_unsigned(data->u.h.choice + 1);
            }
        } else {
            ret += 1;
        }
        if (data->u.h.data != NULL) {
            ret += write_calc_size(data->u.h.data);
        }
        return ret;
    }
    
    if (data->type->kind_code == TSF_TK_ARRAY) {
        if (data->u.c.size >= 255) {
            ret += 5;
        } else {
            ret += 1;
        }
    }
    
    for (i = 0; i < data->u.c.size; ++i) {
        if (data->u.c.array[i]->type->kind_code == TSF_TK_BIT) {
            ++num_bits;
        } else {
            ret += write_calc_size(data->u.c.array[i]);
        }
    }
    
    ret += ((num_bits + 7) >> 3);
    
    return ret;
}

static tsf_bool_t write_mainpass(tsf_reflect_t *data,
                                 tsf_type_out_map_t *type_map,
                                 uint8_t **buf) {
    switch (data->type->kind_code) {
    case TSF_TK_INT8:
    case TSF_TK_UINT8:
    case TSF_TK_BIT:
        copy_htonc_incdst(*buf, &data->u.p_uint8_t);
        break;
    case TSF_TK_INT16:
    case TSF_TK_UINT16:
        copy_htons_incdst(*buf, (uint8_t*)&data->u.p_uint16_t);
        break;
    case TSF_TK_INT32:
    case TSF_TK_UINT32:
        copy_htonl_incdst(*buf, (uint8_t*)&data->u.p_uint32_t);
        break;
    case TSF_TK_INT64:
    case TSF_TK_UINT64:
        copy_htonll_incdst(*buf, (uint8_t*)&data->u.p_uint64_t);
        break;
    case TSF_TK_INTEGER:
        *buf += write_tsf_integer(*buf, data->u.p_integer);
        break;
    case TSF_TK_LONG:
        *buf += write_tsf_long(*buf, data->u.p_long);
        break;
    case TSF_TK_FLOAT:
        copy_htonf_incdst(*buf, (uint8_t*)&data->u.p_float);
        break;
    case TSF_TK_DOUBLE:
        copy_htond_incdst(*buf, (uint8_t*)&data->u.p_double);
        break;
    case TSF_TK_ARRAY: {
        uint8_t tmp;
        if (data->u.c.size >= 255) {
            tmp = (uint8_t)255;
            copy_htonc_incdst(*buf, &tmp);
            copy_htonl_incdst(*buf, (uint8_t*)&data->u.c.size);
        } else {
            tmp = (uint8_t)data->u.c.size;
            copy_htonc_incdst(*buf, &tmp);
        }
        if (tsf_type_get_kind_code(
                tsf_array_type_get_element_type(data->type))
            == TSF_TK_BIT) {
            uint32_t i, j;
            uint8_t value;
            
            for (i = 0; i + 7 < data->u.c.size; i += 8) {
                value = 0;
                for (j = 0; j < 8; ++j) {
                    if (data->u.c.array[i + j]->u.bit) {
                        value |= (1 << j);
                    }
                }
                copy_htonc_incdst(*buf, &value);
            }
            if (data->u.c.size > i) {
                value = 0;
                for (j = 0; j < data->u.c.size - i; ++j) {
                    if (data->u.c.array[i + j]->u.bit) {
                        value |= (1 << j);
                    }
                }
                copy_htonc_incdst(*buf, &value);
            }
        } else {
            uint32_t i;
            for (i = 0; i < data->u.c.size; ++i) {
                if (!write_mainpass(data->u.c.array[i],
                                    type_map,
                                    buf)) {
                    return tsf_false;
                }
            }
        }
        break;
    }
    case TSF_TK_STRUCT: {
        uint32_t i;
        uint8_t value, bit;
        for (i = 0; i < data->u.c.size; ++i) {
            if (data->u.c.array[i]->type->kind_code == TSF_TK_BIT) {
                continue;
            }
            if (!write_mainpass(data->u.c.array[i],
                                type_map,
                                buf)) {
                return tsf_false;
            }
        }
        value = 0;
        bit = 0;
        for (i = 0; i < data->u.c.size; ++i) {
            if (data->u.c.array[i]->type->kind_code != TSF_TK_BIT) {
                continue;
            }
            if (data->u.c.array[i]->u.bit) {
                value |= (1 << bit);
            }
            ++bit;
            if (bit == 8) {
                copy_htonc_incdst(*buf, &value);
                value = 0;
                bit = 0;
            }
        }
        if (bit) {
            copy_htonc_incdst(*buf, &value);
        }
        break;
    }
    case TSF_TK_CHOICE:
        if (tsf_choice_type_get_num_elements(data->type) >= 256) {
            if (data->type->u.h.choice_as_full_word) {
                copy_htonl_incdst(*buf, (uint8_t*)&data->u.h.choice);
            } else {
                *buf += write_tsf_unsigned(*buf, data->u.h.choice + 1);
            }
        } else {
            uint8_t choice_8bit;
            if (data->u.h.choice == UINT32_MAX) {
                choice_8bit = 255;
            } else {
                choice_8bit = data->u.h.choice;
            }
            copy_htonc_incdst(*buf, &choice_8bit);
        }
        if (data->u.h.data != NULL) {
            if (!write_mainpass(data->u.h.data,
                                type_map,
                                buf)) {
                return tsf_false;
            }
        }
        break;
    case TSF_TK_VOID:
        break;
    case TSF_TK_STRING:
#ifdef HAVE_STPCPY
        *buf = (uint8_t*)stpcpy((char*)*buf, data->u.str) + 1;
#else
        strcpy((char*)*buf, data->u.str);
        *buf += strlen(data->u.str) + 1;
#endif
        break;
    case TSF_TK_ANY:
        if (!tsf_buffer_write_to_buf(data->u.any,
                                     type_map,
                                     buf)) {
            return tsf_false;
        }
        break;
    default:
        tsf_set_error(TSF_E_BAD_TYPE_KIND,
                      fui8 " is not a recognized kind code.",
                      data->type->kind_code);
        return tsf_false;
    }
    
    return tsf_true;
}

tsf_buffer_t *tsf_reflect_make_buffer(tsf_reflect_t *data) {
    uint32_t size;
    uint8_t *buf,*cur;
    tsf_type_out_map_t *type_map;
    tsf_type_in_map_t *types;
    
    if (!tsf_reflect_verify(data)) {
        return NULL;
    }
    
    size=write_calc_size(data);

    buf=malloc(size);
    cur=buf;
    
    if (buf==NULL) {
        tsf_set_errno("Could not allocate data for buffer in "
                      "tsf_reflect_make_buffer().");
        return NULL;
    }
    
    if (tsf_type_is_dynamic(data->type)) {
        type_map=tsf_type_out_map_create();
        if (type_map==NULL) {
            free(buf);
            return NULL;
        }
    } else {
        type_map=NULL;
    }
    
    if (!write_mainpass(data,type_map,&cur)) {
        if (type_map!=NULL) {
            tsf_type_out_map_destroy(type_map);
        }
        free(buf);
        return NULL;
    }
    
    if (cur!=buf+size) {
        tsf_set_error(TSF_E_INTERNAL,
            "tsf_reflect_make_buffer() was expected to write "
            fui32 " bytes, but " fsz " bytes were written instead.",
            size,cur-buf);
        if (type_map!=NULL) {
            tsf_type_out_map_destroy(type_map);
        }
        free(buf);
        return NULL;
    }
    
    if (type_map==NULL) {
        types=tsf_type_in_map_get_empty_singleton();
    } else {
        types=tsf_type_in_map_from_type_out_map(type_map);
        if (types==NULL) {
            free(buf);
            return NULL;
        }
    }
    
    return tsf_buffer_create(data->type,types,buf,size,tsf_true);
}

tsf_reflect_t *read_mainpass(tsf_type_t *type,
                             tsf_type_in_map_t *types,
                             uint8_t **buf,
                             uint8_t *end) {
    tsf_reflect_t *ret = NULL;    /* make GCC happy */
    uint8_t ui8_tmp;
    int8_t i8_tmp;
    
    /* must special-case singletons!! */
    switch (type->kind_code) {
    case TSF_TK_INT8:
        copy_ntohc_incsrc_bc((uint8_t*)&i8_tmp, *buf,
                             end,singleton_bounds_error);
        return int8_data + i8_tmp + 128;
    case TSF_TK_UINT8:
        copy_ntohc_incsrc_bc(&ui8_tmp, *buf, end, singleton_bounds_error);
        return uint8_data + ui8_tmp;
    case TSF_TK_BIT:
        copy_ntohc_incsrc_bc(&ui8_tmp, *buf, end, singleton_bounds_error);
        if (ui8_tmp != 0) {
            return &bit_data_true;
        }
        return &bit_data_false;
    case TSF_TK_VOID:
        return &void_data;
    default:
        break;
    }
    
    ret=malloc(sizeof(tsf_reflect_t));
    if (ret==NULL) {
        tsf_set_errno("Could not allocate tsf_reflect_t");
        return NULL;
    }
    
    ret->type=tsf_type_dup(type);
    
    switch (type->kind_code) {
    case TSF_TK_INT8:
    case TSF_TK_UINT8:
        tsf_abort("Should have been handled above.");
        break;
    case TSF_TK_INT16:
    case TSF_TK_UINT16:
        copy_ntohs_incsrc_bc((uint8_t*)&ret->u.p_uint16_t, *buf,
                             end, bounds_error);
        break;
    case TSF_TK_INT32:
    case TSF_TK_UINT32:
        copy_ntohl_incsrc_bc((uint8_t*)&ret->u.p_uint32_t, *buf,
                             end, bounds_error);
        break;
    case TSF_TK_INT64:
    case TSF_TK_UINT64:
        copy_ntohll_incsrc_bc((uint8_t*)&ret->u.p_uint64_t, *buf,
                              end, bounds_error);
        break;
    case TSF_TK_INTEGER:
        read_tsf_integer_incsrc(&ret->u.p_integer, *buf, end, bounds_error);
        break;
    case TSF_TK_LONG:
        read_tsf_long_incsrc(&ret->u.p_long, *buf, end, bounds_error);
        break;
    case TSF_TK_FLOAT:
        copy_ntohf_incsrc_bc((uint8_t*)&ret->u.p_float, *buf,
                             end, bounds_error);
        break;
    case TSF_TK_DOUBLE:
        copy_ntohd_incsrc_bc((uint8_t*)&ret->u.p_double, *buf,
                             end, bounds_error);
        break;
    case TSF_TK_BIT:
        tsf_abort("Should have been handled above.");
        break;
    case TSF_TK_ARRAY: {
        uint32_t i;
        tsf_type_t *ele_type = tsf_array_type_get_element_type(type);
        uint8_t tmp;

        copy_ntohc_incsrc_bc(&tmp, *buf, end, bounds_error);
        if (tmp == 255) {
            copy_ntohl_incsrc_bc((uint8_t*)&ret->u.c.size, *buf,
                                 end, bounds_error);
        } else {
            ret->u.c.size = tmp;
        }
                
        ret->u.c.array = malloc(sizeof(tsf_reflect_t*) *
                                ret->u.c.size);
        if (ret->u.c.array == NULL) {
            tsf_set_errno("Could not allocate arrray of pointers "
                          "in tsf_reflect_from_buffer_impl()");
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
                
        if (tsf_type_get_kind_code(ele_type) == TSF_TK_BIT) {
            uint32_t i,j,k;
            uint8_t value;
                    
            for (i = 0; i + 7 < ret->u.c.size; i += 8) {
                copy_ntohc_incsrc_bc(&value, *buf,
                                     end, bits_bounds_error);
                for (j = 0; j < 8; ++j) {
                    ret->u.c.array[i + j] =
                        tsf_reflect_create_bit((value & (1 << j)) != 0);
                    if (ret->u.c.array[i + j] == NULL) {
                        for (k = 0; k < i + j; ++k) {
                            tsf_reflect_destroy(ret->u.c.array[k]);
                        }
                        tsf_type_destroy(ret->type);
                        free(ret->u.c.array);
                        free(ret);
                        return NULL;
                    }
                }
            }
            if (ret->u.c.size > i) {
                copy_ntohc_incsrc_bc(&value, *buf,
                                     end, bits_bounds_error);
                for (j = 0; j < ret->u.c.size - i; ++j) {
                    ret->u.c.array[i + j] =
                        tsf_reflect_create_bit((value & (1 << j)) != 0);
                    if (ret->u.c.array[i + j] == NULL) {
                        for (k = 0; k < i + j; ++k) {
                            tsf_reflect_destroy(ret->u.c.array[k]);
                        }
                        tsf_type_destroy(ret->type);
                        free(ret->u.c.array);
                        free(ret);
                        return NULL;
                    }
                }
            }
                    
            break;
        bits_bounds_error:
            for (j = 0; j < i; ++j) {
                tsf_reflect_destroy(ret->u.c.array[j]);
            }
            free(ret->u.c.array);
            goto bounds_error;
        }
                
        for (i = 0; i < ret->u.c.size; ++i) {
            ret->u.c.array[i] = read_mainpass(ele_type, types, buf, end);
            if (ret->u.c.array[i] == NULL) {
                uint32_t j;
                for (j = 0; j < i; ++j) {
                    tsf_reflect_destroy(ret->u.c.array[j]);
                }
                free(ret->u.c.array);
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
        }
        break;
    }
    case TSF_TK_STRUCT: {
        uint32_t i;
        uint8_t bit;
        uint8_t value = 0; /* assignment only to make gcc happy */
        tsf_named_type_t *n;
                
        ret->u.c.size = tsf_struct_type_get_num_elements(type);
        ret->u.c.array = malloc(sizeof(tsf_reflect_t*) *
                                tsf_struct_type_get_num_elements(type));
                
        if (ret->u.c.array == NULL) {
            tsf_set_errno("Could not allocate arrray of pointers "
                          "in tsf_reflect_from_buffer_impl()");
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
                
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            n = tsf_struct_type_get_element(type, i);
            if (n->type->kind_code == TSF_TK_BIT) {
                continue;
            }
            ret->u.c.array[i] = read_mainpass(n->type, types, buf, end);
            if (ret->u.c.array[i] == NULL) {
                uint32_t j;
                for (j = 0; j < i; ++j) {
                    n = tsf_struct_type_get_element(type,j);
                    if (n->type->kind_code == TSF_TK_BIT) {
                        continue;
                    }
                    tsf_reflect_destroy(ret->u.c.array[j]);
                }
                free(ret->u.c.array);
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
        }
        bit = 8;
        for (i = 0; i < tsf_struct_type_get_num_elements(type); ++i) {
            n = tsf_struct_type_get_element(type, i);
            if (n->type->kind_code != TSF_TK_BIT) {
                continue;
            }
            if (bit == 8) {
                copy_ntohc_incsrc_bc(&value, *buf, end, bounds_error);
                bit = 0;
            }
            ret->u.c.array[i] =
                tsf_reflect_create_bit((value & (1 << bit)));
            if (ret->u.c.array[i] == NULL) {
                uint32_t j;
                for (j = 0; j < i; ++j) {
                    tsf_reflect_destroy(ret->u.c.array[i]);
                }
                free(ret->u.c.array);
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
            ++bit;
        }
        break;
    }
    case TSF_TK_CHOICE:
        if (tsf_choice_type_get_num_elements(type) >= 256) {
            if (type->u.h.choice_as_full_word) {
                copy_htonl_incsrc_bc((uint8_t*)&ret->u.h.choice, *buf,
                                     end, bounds_error);
            } else {
                tsf_unsigned_t choice_value;
                read_tsf_unsigned_incsrc(&choice_value, *buf,
                                         end, bounds_error);
                ret->u.h.choice = choice_value - 1;
            }
        } else {
            uint8_t choice_8bit;
            copy_htonc_incsrc_bc(&choice_8bit, *buf,
                                 end, bounds_error);
            ret->u.h.choice =
                choice_8bit == 255 ? UINT32_MAX : choice_8bit;
        }
        if (ret->u.h.choice != UINT32_MAX) {
            if (ret->u.h.choice >= tsf_choice_type_get_num_elements(type)) {
                tsf_set_error(TSF_E_PARSE_ERROR,
                              "Choice is out of bounds of what we know: "
                              "the value we see is " fui32 " but the "
                              "number of elements in the choice type is "
                              fui32,
                              ret->u.h.choice,
                              tsf_choice_type_get_num_elements(type));
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
            ret->u.h.data = read_mainpass(
                tsf_choice_type_get_element(type, ret->u.h.choice)->type,
                types, buf, end);
            if (ret->u.h.data == NULL) {
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
        } else {
            ret->u.h.data = NULL;
        }
        break;
    case TSF_TK_VOID:
        tsf_abort("Should have been handled above.");
        break;
    case TSF_TK_STRING:
    {
        const size_t step = 128;
        size_t size = step;
        uint32_t i = 0;
        ret->u.str = malloc(size);
        for (;;) {
            uint32_t n;
                    
            if (ret->u.str == NULL) {
                tsf_set_errno("Could not (m|re)alloc string");
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
                    
            for (n = step; n-->0;) {
                uint8_t c;
                copy_ntohc_incsrc_bc(&c, *buf, end, bounds_error);
                ret->u.str[i++] = c;
                if (c == 0) {
                    return ret;
                }
            }
                    
            size += step;
            ret->u.str = realloc(ret->u.str, size);
        }
        tsf_assert(!"unreachable");
        break;
    }
    case TSF_TK_ANY: {
        ret->u.any = tsf_buffer_read_from_buf(types,
                                              buf,
                                              end,
                                              NULL);
        if (ret->u.any == NULL) {
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
        break;
    }
    default:
        tsf_set_error(TSF_E_INTERNAL,
                      fui32 " is not a recognized kind code.",
                      type->kind_code);
        tsf_type_destroy(ret->type);
        free(ret);
        return NULL;
    }
    
    increment_reflect_balance();
    
    return ret;

bounds_error:
    tsf_type_destroy(ret->type);
    free(ret);
singleton_bounds_error:
    tsf_set_error(TSF_E_PARSE_ERROR,
                  "Bounds error in reflecting parser");
    return NULL;
}

tsf_reflect_t *tsf_reflect_from_buffer(tsf_buffer_t *buf) {
    uint8_t *data;
    tsf_reflect_t *ret;

    check_init_reflect();

    data=buf->data;
    
    ret=read_mainpass(buf->type,
                      buf->types,
                      &data,
                      ((uint8_t*)buf->data)+buf->size);
    if (ret==NULL) {
        return NULL;
    }
    
    if (data!=((uint8_t*)buf->data)+buf->size) {
        tsf_set_error(TSF_E_PARSE_ERROR,
            "tsf_reflect_from_buffer_impl() was expected to read "
            fui32 " bytes, but " fsz " bytes were read instead.",
            buf->size,((uint8_t*)data)-((uint8_t*)buf->data));
        tsf_reflect_destroy(ret);
        return NULL;
    }
    
    return ret;
}

tsf_type_t *tsf_reflect_get_type(tsf_reflect_t *data) {
    return data->type;
}

tsf_type_kind_t tsf_reflect_get_kind_code(tsf_reflect_t *data) {
    return tsf_type_get_kind_code(tsf_reflect_get_type(data));
}

static tsf_bool_t struct_set_element_impl(tsf_named_type_t *n,
                                          tsf_reflect_t *data,
                                          tsf_reflect_t *ele_data) {
    uint32_t index;

    if (!tsf_type_compare(n->type,ele_data->type)) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "Third argument to tsf_struct_set_element() does "
            "not have the right type.");
        tsf_reflect_destroy(ele_data);
        return tsf_false;
    }
    
    index=tsf_named_type_get_index(n);
    
    if (data->u.c.array[index]!=NULL) {
        tsf_reflect_destroy(data->u.c.array[index]);
    }
    data->u.c.array[index]=ele_data;
    return tsf_true;
}

tsf_bool_t tsf_struct_reflect_set_element(tsf_reflect_t *data,
                                          const char *name,
                                          tsf_reflect_t *ele_data) {
    tsf_named_type_t *n;
    
    if (ele_data==NULL) {
        return tsf_false;
    }
    
    if (data->type->kind_code!=TSF_TK_STRUCT) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "First argument to tsf_struct_reflect_set_element() is "
            "not a struct.");
        tsf_reflect_destroy(ele_data);
        return tsf_false;
    }
    
    n=tsf_struct_type_find_node(data->type,name);
    if (n==NULL) {
        tsf_reflect_destroy(ele_data);
        return tsf_false;
    }

    return struct_set_element_impl(n,data,ele_data);
}

tsf_bool_t tsf_struct_reflect_set_element_by_index(tsf_reflect_t *data,
                                                   uint32_t index,
                                                   tsf_reflect_t *ele_data) {
    if (ele_data==NULL) {
        return tsf_false;
    }

    if (data->type->kind_code!=TSF_TK_STRUCT) {
        tsf_set_error(TSF_E_BAD_TYPE,
                      "First argument to "
                      "tsf_struct_reflect_set_element_by_index() is not a "
                      "struct.");
        tsf_reflect_destroy(ele_data);
        return tsf_false;
    }
    
    if (index>=tsf_struct_type_get_num_elements(data->type)) {
        tsf_set_error(TSF_E_INVALID_ARG,
                      "Index " fui32 " is out of bounds of valid element "
                      "indices (upper bound is " fui32 ") in "
                      "tsf_struct_reflect_set_element_by_index()",
                      index,
                      tsf_struct_type_get_num_elements(data->type));
        return tsf_false;
    }

    return struct_set_element_impl(tsf_struct_type_get_element(data->type,
                                                               index),
                                   data,
                                   ele_data);
}

tsf_reflect_t *tsf_struct_reflect_get_element(tsf_reflect_t *data,
                                              const char *name) {
    tsf_named_type_t *n;

    if (data->type->kind_code!=TSF_TK_STRUCT) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "First argument to tsf_struct_reflect_get_element() is "
            "not a struct.");
        return tsf_false;
    }
    
    n=tsf_struct_type_find_node(data->type,name);
    if (n==NULL) {
        return NULL;
    }
    
    return data->u.c.array[tsf_named_type_get_index(n)];
}

tsf_reflect_t *tsf_struct_reflect_get_element_by_index(tsf_reflect_t *data,
                                                       uint32_t index) {
    if (data->type->kind_code!=TSF_TK_STRUCT) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "First argument to tsf_struct_reflect_get_element() is "
            "not a struct.");
        return tsf_false;
    }
    
    if (index>=tsf_struct_type_get_num_elements(data->type)) {
        tsf_set_error(TSF_E_INVALID_ARG,
                      "Index " fui32 " is out of bounds of valid element "
                      "indices (upper bound is " fui32 ") in "
                      "tsf_struct_reflect_get_element_by_index()",
                      index,
                      tsf_struct_type_get_num_elements(data->type));
        return tsf_false;
    }
    
    return data->u.c.array[index];
}

uint32_t tsf_struct_eflect_get_num_elements(tsf_reflect_t *data) {
    return tsf_struct_type_get_num_elements(tsf_reflect_get_type(data));
}

tsf_bool_t tsf_array_reflect_append(tsf_reflect_t *data,
                                    tsf_reflect_t *ele_data) {
    if (ele_data==NULL) {
        return tsf_false;
    }
    
    if (data->type->kind_code!=TSF_TK_ARRAY) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "First argument to tsf_array_reflect_append() is "
            "not an array.");
        return tsf_false;
    }
    
    if (!tsf_type_compare(ele_data->type,
            tsf_array_type_get_element_type(data->type))) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "Second argument to tsf_array_reflect_append() does "
            "match the element type of the array.");
        return tsf_false;
    }
    
    if (data->u.c.array==NULL) {
        data->u.c.array=malloc(sizeof(tsf_reflect_t*));
        if (data->u.c.array==NULL) {
            tsf_set_errno("Could not allocate array of tsf_reflect_t*");
            return tsf_false;
        }
    } else {
        tsf_reflect_t **new_array=realloc(data->u.c.array,
                                          sizeof(tsf_reflect_t*)*
                                          (data->u.c.size+1));
        if (new_array==NULL) {
            tsf_set_errno("Could not allocate array of tsf_reflect_t*");
            return tsf_false;
        }
        data->u.c.array=new_array;
    }
    
    data->u.c.array[data->u.c.size++]=ele_data;
    
    return tsf_true;
}

static tsf_bool_t choice_set_impl(tsf_named_type_t *n,
                                  tsf_reflect_t *data,
                                  tsf_reflect_t *ele_data) {
    if (!tsf_type_compare(n->type,ele_data->type)) {
        tsf_set_error(TSF_E_BAD_TYPE,
            "Third argument to tsf_choice_reflect_set() does "
            "not have the correct type.");
        tsf_reflect_destroy(ele_data);
        return tsf_false;
    }

    if (data->u.h.data!=NULL) {
        tsf_reflect_destroy(data->u.h.data);
    }
    
    data->u.h.choice=n->index;
    data->u.h.data=ele_data;
    
    return tsf_true;
}

tsf_bool_t tsf_choice_reflect_set(tsf_reflect_t *data,
                                  const char *choice,
                                  tsf_reflect_t *ele_data) {
    tsf_named_type_t *n;
    
    if (choice==NULL) {
        return tsf_choice_reflect_set_unknown(data);
    }

    if (ele_data==NULL) {
        return tsf_false;
    }
    
    n=tsf_choice_type_find_node(data->type,choice);
    if (n==NULL) {
        tsf_reflect_destroy(ele_data);
        return tsf_false;
    }
    
    return choice_set_impl(n,data,ele_data);
}

tsf_bool_t tsf_choice_reflect_set_by_index(tsf_reflect_t *data,
                                           uint32_t index,
                                           tsf_reflect_t *ele_data) {
    if (index==UINT32_MAX) {
        return tsf_choice_reflect_set_unknown(data);
    }

    if (ele_data==NULL) {
        return tsf_false;
    }

    if (index>=tsf_choice_type_get_num_elements(data->type)) {
        tsf_set_error(TSF_E_INVALID_ARG,
                      "Index " fui32 " is out of bounds of valid element "
                      "indices (upper bound is " fui32 ") in "
                      "tsf_choice_reflect_set_by_index()",
                      index,
                      tsf_struct_type_get_num_elements(data->type));
        return tsf_false;
    }
    
    return choice_set_impl(tsf_choice_type_get_element(data->type,
                                                       index),
                           data,
                           ele_data);
}

tsf_bool_t tsf_choice_reflect_set_unknown(tsf_reflect_t *data) {
    if (data->u.h.data!=NULL) {
        tsf_reflect_destroy(data->u.h.data);
    }
    
    data->u.h.choice=UINT32_MAX;
    data->u.h.data=NULL;
    
    return tsf_true;
}

uint32_t tsf_choice_reflect_get_index(tsf_reflect_t *data) {
    return data->u.h.choice;
}

tsf_bool_t tsf_choice_reflect_is_known(tsf_reflect_t *data) {
    return tsf_choice_reflect_get_index(data) != TSF_UINT32_MAX;
}

const char *tsf_choice_reflect_get_choice(tsf_reflect_t *data) {
    return tsf_choice_reflect_is_known(data) ?
        tsf_choice_type_get_element(data->type,
                                    data->u.h.choice)->name :
        NULL;
}

tsf_reflect_t *tsf_choice_reflect_get_data(tsf_reflect_t *data) {
    return data->u.h.data;
}

tsf_reflect_t *tsf_array_reflect_get_element(tsf_reflect_t *data,
                                             uint32_t index) {
    return data->u.c.array[index];
}

uint32_t tsf_array_reflect_get_num_elements(tsf_reflect_t *data) {
    return data->u.c.size;
}

tsf_bool_t tsf_bit_reflect_get(tsf_reflect_t *data) {
    return data->u.bit;
}

const char* tsf_string_reflect_get(tsf_reflect_t *data) {
    return data->u.str;
}

tsf_buffer_t *tsf_any_reflect_get_buffer(tsf_reflect_t *data) {
    return data->u.any;
}


