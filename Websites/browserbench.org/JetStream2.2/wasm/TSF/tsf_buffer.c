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

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#ifdef DEBUG_BUFFER_ALLOC
static int64_t buffer_balance = 0;

int64_t tsf_buffer_get_balance() {
    return buffer_balance;
}

#define increment_buffer_balance() do {\
    buffer_balance++;\
} while(0)

#define decrement_buffer_balance() do {\
    buffer_balance--;\
} while(0)

#else

#define increment_buffer_balance() do {\
} while(0)

#define decrement_buffer_balance() do {\
} while(0)

#endif

static tsf_buffer_t empty_buffer;

static void init_empty_buffer(void) {
    empty_buffer.type=tsf_type_create(TSF_TK_VOID);
    empty_buffer.types=tsf_type_in_map_get_empty_singleton();
    empty_buffer.data=NULL;
    empty_buffer.size=0;
    empty_buffer.keep=tsf_false;
}

static tsf_once_t once_control = TSF_ONCE_INIT;

static void check_init_empty_buffer(void) {
    tsf_once(&once_control, init_empty_buffer);
}

tsf_buffer_t *tsf_buffer_get_empty_singleton(void) {
    check_init_empty_buffer();
    return &empty_buffer;
}

tsf_buffer_t *tsf_buffer_create(tsf_type_t *type,
                                tsf_type_in_map_t *types,
                                void *data,
                                uint32_t size,
                                tsf_bool_t keep) {
    tsf_buffer_t *ret=malloc(sizeof(tsf_buffer_t));
    if (ret==NULL) {
        tsf_set_errno("Could not allocate tsf_buffer_t struct.");
        if (keep) {
            free(data);
        }
        tsf_type_in_map_destroy(types);
        return NULL;
    }
    
    ret->type=tsf_type_dup(type);
    ret->types=types;
    ret->data=data;
    ret->size=size;
    ret->keep=keep;
    
    tsf_atomic_count_init(&ret->ref_count, 1);
    
    increment_buffer_balance();
    
    return ret;
}

void tsf_buffer_initialize_custom(tsf_buffer_t *buf,
                                  void *backing_store,
                                  size_t backing_store_size) {
    buf->data = backing_store;
    buf->size = backing_store_size;
    buf->keep = tsf_false;
    tsf_atomic_count_init(&buf->ref_count, 0);
}

tsf_buffer_t *tsf_buffer_create_bare(void) {
    tsf_buffer_t *result = malloc(sizeof(tsf_buffer_t));
    if (result == NULL) {
        tsf_set_errno("Could not malloc a tsf_buffer_t struct.");
        return NULL;
    }
    
    result->data = NULL;
    result->size = 0;
    result->keep = tsf_false;

    tsf_atomic_count_init(&result->ref_count, 1);
    
    increment_buffer_balance();
    
    return result;
}

tsf_buffer_t *tsf_buffer_create_bare_in(void *region) {
    tsf_buffer_t *result = tsf_region_alloc(region, sizeof(tsf_buffer_t));
    if (result == NULL) {
        tsf_set_errno("Could not tsf_region_alloc a tsf_buffer_t struct.");
        return NULL;
    }
    
    result->data = NULL;
    result->size = 0;
    result->keep = tsf_false;

    tsf_atomic_count_init(&result->ref_count, 0);
    
    increment_buffer_balance();
    
    return result;
}

tsf_buffer_t *tsf_buffer_create_in(tsf_type_t *type,
                                   tsf_type_in_map_t *types,
                                   void *data,
                                   uint32_t size,
                                   void *region) {
    tsf_buffer_t *ret=tsf_region_alloc(region,sizeof(tsf_buffer_t));
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_region_alloc tsf_buffer_t struct.");
        return NULL;
    }
    
    if (!tsf_region_add_cback(region,
                              (tsf_region_cback_t)tsf_type_destroy,
                              type)) {
        tsf_set_errno("Could not tsf_region_add_cback for "
                      "tsf_type_destroy");
        return NULL;
    }
    
    ret->type=tsf_type_dup(type);
    ret->types=types;
    ret->data=data;
    ret->size=size;
    ret->keep=tsf_false;
    
    tsf_atomic_count_init(&ret->ref_count, 0);

    return ret;
}

tsf_buffer_t *tsf_buffer_dup(tsf_buffer_t *buf) {
    if (buf==&empty_buffer) {
	return buf;
    }
    
    /* Keep being false means that someone else owns the backing store. Null reference count means 
       that this buffer is owned by someone else. */
    if (!buf->keep || !tsf_atomic_count_value(&buf->ref_count)) {
        return tsf_buffer_clone(buf);
    }
    
    tsf_atomic_count_xadd(&buf->ref_count, 1);
    
    increment_buffer_balance();
    
    return buf;
}

tsf_buffer_t *tsf_buffer_clone(tsf_buffer_t *buf) {
    tsf_buffer_t *ret=malloc(sizeof(tsf_buffer_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc tsf_buffer_t");
        return NULL;
    }
    
    ret->size=buf->size;
    ret->data=malloc(buf->size);
    if (ret->data==NULL) {
        free(ret);
        tsf_set_errno("Could not malloc buffer");
        return NULL;
    }
    
    memcpy(ret->data,buf->data,ret->size);
    
    ret->type=tsf_type_dup(buf->type);
    ret->types=tsf_type_in_map_clone(buf->types);
    if (ret->types==NULL) {
        tsf_type_destroy(ret->type);
        free(ret->data);
        free(ret);
        return NULL;
    }
    
    ret->keep=tsf_true;
    
    tsf_atomic_count_init(&ret->ref_count, 1);
    
    increment_buffer_balance();
    
    return ret;
}

tsf_buffer_t *tsf_buffer_clone_in(tsf_buffer_t *buf,
                                  void *region) {
    tsf_buffer_t *ret=tsf_region_alloc(region,sizeof(tsf_buffer_t));
    if (ret==NULL) {
        tsf_set_errno("Could not tsf_region_alloc tsf_buffer_t");
        return NULL;
    }
    
    ret->size=buf->size;
    ret->data=tsf_region_alloc(region,buf->size);
    if (ret->data==NULL) {
        tsf_set_errno("Could not tsf_region_alloc buffer");
        return NULL;
    }
    
    memcpy(ret->data,buf->data,ret->size);

    if (!tsf_region_add_cback(region,
                              (tsf_region_cback_t)tsf_type_destroy,
                              buf->type)) {
        tsf_set_errno("Could not tsf_region_add_cback for "
                      "tsf_type_destroy");
        return NULL;
    }
    
    ret->type=tsf_type_dup(buf->type);
    ret->types=tsf_type_in_map_clone_in(buf->types,region);
    if (ret->types==NULL) {
        return NULL;
    }
    
    ret->keep=tsf_false;
    
    return ret;
}

static void destroy_buffer_contents(tsf_buffer_t *buf) {
    if (buf->keep) {
        free(buf->data);
    }

    tsf_atomic_count_destroy(&buf->ref_count);
    tsf_type_destroy(buf->type);
    tsf_type_in_map_destroy(buf->types);
}

void tsf_buffer_destroy_custom(tsf_buffer_t *buf) {
    tsf_assert(tsf_atomic_count_value(&buf->ref_count) == 0);
    destroy_buffer_contents(buf);
}

void tsf_buffer_destroy(tsf_buffer_t *buf) {
    if (buf==&empty_buffer) {
	return;
    }
    
    tsf_assert(tsf_atomic_count_value(&buf->ref_count));
    
    decrement_buffer_balance();
    
    if (tsf_atomic_count_xsub(&buf->ref_count, 1) > 1) {
        return;
    }
    
    destroy_buffer_contents(buf);
    free(buf);
}

void tsf_buffer_destroy_bare(tsf_buffer_t *buf) {
    free(buf);
}

uint32_t tsf_buffer_get_size(tsf_buffer_t *buf) {
    return buf->size;
}

const void *tsf_buffer_get_data(tsf_buffer_t *buf) {
    return buf->data;
}

typedef enum {
    small_length,
    large_length
} length_size_t;

static tsf_bool_t read_buffer_impl(tsf_buffer_t *result,
                                   uint32_t type_code,
                                   tsf_type_in_map_t *enclosing_types,
                                   tsf_reader_t reader,
                                   void *arg,
                                   tsf_limits_t *limits,
                                   length_size_t length_size) {
    uint32_t size;
    
    if (type_code >= tsf_type_in_map_get_num_types(enclosing_types)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Type code is outside of bounds of type map: "
                      fui32 " is the type code, while the type map "
                      "has a size of " fui32,
                      type_code,
                      tsf_type_in_map_get_num_types(enclosing_types));
        goto error1;
    }
    
    result->type = tsf_type_dup(
        tsf_type_in_map_get_type(enclosing_types, type_code));
    
    if (tsf_type_is_dynamic(result->type)) {
        /* must read type translation table. */
        uint32_t num_types;
        uint32_t index;
        
        result->types = tsf_type_in_map_create();
        if (result->types == NULL) {
            goto error2;
        }
        
        if (!reader(arg, &num_types, sizeof(num_types))) {
            goto error3;
        }
        
        num_types = ntohl(num_types);
        
        tsf_type_in_map_prepare(result->types, num_types);
        
        while (num_types --> 0) {
            if (!reader(arg, &index, sizeof(index))) {
                goto error3;
            }
            
            index = ntohl(index);
            
            if (index >= tsf_type_in_map_get_num_types(enclosing_types)) {
                tsf_set_error(TSF_E_PARSE_ERROR,
                              "Index " fui32 " into enclosing type table "
                              "is outside of bounds; the enclosing table "
                              "has a size of " fui32,
                              index,
                              tsf_type_in_map_get_num_types(enclosing_types));
                goto error3;
            }
            
            if (!tsf_type_in_map_append(
                    result->types,
                    tsf_type_in_map_get_type(enclosing_types, index))) {
                goto error3;
            }
        }
    } else {
        result->types = tsf_type_in_map_get_empty_singleton();
    }

    switch (length_size) {
    case small_length: {
        uint8_t my_size;
        if (!reader(arg, &my_size, sizeof(my_size))) {
            goto error3;
        }
        size = my_size;
        break;
    }
    case large_length: {
        if (!reader(arg, &size, sizeof(size))) {
            goto error3;
        }
        size = ntohl(size);
        break;
    } }
    
    if (size > tsf_limits_max_buf_size(limits)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Buffer size exceeds limit");
        goto error3;
    }
    
    if (size > result->size) {
        result->data = malloc(size);
        if (result->data == NULL) {
            tsf_set_errno("Could not allocate buffer data.");
            goto error3;
        }
        
        result->keep = tsf_true;
    }
    
    result->size = size;
    
    if (!reader(arg, result->data, result->size)) {
        goto error4;
    }
    
    return tsf_true;

error4:
    free(result->data);
    result->keep = tsf_false;
error3:
    tsf_type_in_map_destroy(result->types);
error2:
    tsf_type_destroy(result->type);
error1:
    return tsf_false;
}

tsf_bool_t tsf_buffer_read_simple(tsf_buffer_t *buf,
                                  tsf_type_in_map_t *enclosing_types,
                                  tsf_reader_t reader,
                                  void *arg,
                                  tsf_limits_t *limits) {
    uint32_t type_code;
    
    if (!reader(arg, &type_code, sizeof(type_code))) {
        return tsf_false;
    }
    
    return read_buffer_impl(buf,
                            ntohl(type_code),
                            enclosing_types,
                            reader,
                            arg,
                            limits,
                            large_length);
}

tsf_bool_t tsf_buffer_read_small_with_type_code(tsf_buffer_t *buf,
                                                uint32_t type_code,
                                                tsf_type_in_map_t *enclosing_types,
                                                tsf_reader_t reader,
                                                void *arg,
                                                tsf_limits_t *limits) {
    return read_buffer_impl(buf,
                            type_code,
                            enclosing_types,
                            reader,
                            arg,
                            limits,
                            small_length);
}

tsf_buffer_t *tsf_buffer_read_from_buf(tsf_type_in_map_t *enclosing_types,
                                       uint8_t **cur,
                                       uint8_t *end,
                                       void *region) {
    uint32_t type_code;
    tsf_type_t *type;
    tsf_buffer_t *ret = tsf_cond_alloc(region, sizeof(tsf_buffer_t));
    if (ret == NULL) {
        tsf_set_errno("Could not tsf_cond_alloc tsf_buffer_t");
        return NULL;
    }
    
    ret->keep = (region == NULL ? tsf_true : tsf_false);
    
    copy_ntohl_incsrc_bc((uint8_t*)&type_code, *cur, end, phase1_bounds_error);
    
    if (type_code >= tsf_type_in_map_get_num_types(enclosing_types)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Type code is outside of bounds of type map: "
                      fui32 " is the type code, while the type map "
                      "has a size of " fui32,
                      type_code,
                      tsf_type_in_map_get_num_types(enclosing_types));
        if (region == NULL) {
            free(ret);
        }
        return NULL;
    }
    
    type = tsf_type_in_map_get_type(enclosing_types, type_code);
    
    if (region != NULL) {
        if (!tsf_region_add_cback(region,
                                  (tsf_region_cback_t)tsf_type_destroy,
                                  type)) {
            tsf_set_errno("Could not tsf_region_add_cback for "
                          "tsf_type_destroy");
            return NULL;
        }
    }
    
    ret->type = tsf_type_dup(type);
    
    if (tsf_type_is_dynamic(ret->type)) {
        /* must read type translation table. */
        uint32_t num_types;
        uint32_t index;
        
        if (region == NULL) {
            ret->types = tsf_type_in_map_create();
            if (ret->types == NULL) {
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
        } else {
            ret->types = tsf_type_in_map_create_in(region);
            if (ret->types == NULL) {
                return NULL;
            }
        }
        
        copy_ntohl_incsrc_bc((uint8_t*)&num_types,
                             *cur, end, phase2_bounds_error);
        
        tsf_type_in_map_prepare(ret->types, num_types);
        
        while (num_types --> 0) {
            copy_ntohl_incsrc_bc((uint8_t*)&index,
                                 *cur, end, phase2_bounds_error);
            
            if (index >= tsf_type_in_map_get_num_types(enclosing_types)) {
                tsf_set_error(TSF_E_PARSE_ERROR,
                              "Index " fui32 " into enclosing type table "
                              "is outside of bounds; the enclosing table "
                              "has a size of " fui32,
                              index,
                              tsf_type_in_map_get_num_types(enclosing_types));
                if (region == NULL) {
                    tsf_type_in_map_destroy(ret->types);
                    tsf_type_destroy(ret->type);
                    free(ret);
                }
                return NULL;
            }
            
            if (!tsf_type_in_map_append(
                     ret->types,
                     tsf_type_in_map_get_type(enclosing_types, index))) {
                if (region == NULL) {
                    tsf_type_in_map_destroy(ret->types);
                    tsf_type_destroy(ret->type);
                    free(ret);
                }
                return NULL;
            }
        }
    } else {
        ret->types = tsf_type_in_map_get_empty_singleton();
    }
    
    copy_ntohl_incsrc_bc((uint8_t*)&ret->size,
                         *cur, end, phase2_bounds_error);
    
    if (ret->size + *cur > end) {
        goto phase2_bounds_error;
    }
    
    ret->data = tsf_cond_alloc(region, ret->size);
    if (ret->data == NULL) {
        if (region == NULL) {
            tsf_type_in_map_destroy(ret->types);
            tsf_type_destroy(ret->type);
            free(ret);
        }
        tsf_set_errno("Could not tsf_cond_alloc buffer");
        return NULL;
    }
    
    memcpy(ret->data, *cur, ret->size);
    (*cur) += ret->size;
    
    if (region == NULL) {
        tsf_atomic_count_init(&ret->ref_count, 1);
        increment_buffer_balance();
    }
    
    return ret;

phase2_bounds_error:
    if (region == NULL) {
        tsf_type_destroy(ret->type);
        tsf_type_in_map_destroy(ret->types);
    }
phase1_bounds_error:
    if (region == NULL) {
        free(ret);
    }
    tsf_set_error(TSF_E_PARSE_ERROR,
                  "Bounds error in tsf_buffer_read_from_buf()");
    return NULL;
}

tsf_bool_t tsf_buffer_skip_in_buf(tsf_type_in_map_t *enclosing_types,
                                  uint8_t **cur,
                                  uint8_t *end) {
    uint32_t type_code,size;
    
    copy_ntohl_incsrc_bc((uint8_t*)&type_code,*cur,end,bounds_error);
    
    if (type_code>=tsf_type_in_map_get_num_types(enclosing_types)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Type code is outside of bounds of type map: "
                      fui32 " is the type code, while the type map "
                      "has a size of " fui32,
                      type_code,
                      tsf_type_in_map_get_num_types(enclosing_types));
        return tsf_false;
    }
    
    if (tsf_type_is_dynamic(
            tsf_type_in_map_get_type(enclosing_types,type_code))) {
        /* must skip type translation table. */
        uint32_t num_types;
        
        copy_ntohl_incsrc_bc((uint8_t*)&num_types,*cur,end,bounds_error);
        
        (*cur)+=num_types*4;
    }

    copy_ntohl_incsrc_bc((uint8_t*)&size,*cur,end,bounds_error);
    
    if (size + *cur > end) {
        goto bounds_error;
    }
    
    (*cur)+=size;
    
    return tsf_true;
    
bounds_error:
    tsf_set_error(TSF_E_PARSE_ERROR,
                  "Bounds error in tsf_buffer_skip_in_buf()");
    return tsf_false;
}

uint32_t tsf_buffer_calc_size(tsf_buffer_t *buf) {
    return (tsf_type_is_dynamic(buf->type) ?
            4 + 4 * tsf_type_in_map_get_num_types(buf->types) : 0) +
        buf->size + 4 + 4;
}

static tsf_bool_t write_buffer_impl(tsf_buffer_t *buf,
                                    tsf_type_out_map_t *type_map,
                                    tsf_writer_t writer,
                                    void *arg,
                                    length_size_t length_size) {
    if (tsf_type_is_dynamic(buf->type)) {
        uint32_t num, i, size_word;
        num = tsf_type_in_map_get_num_types(buf->types);
        size_word = htonl(num);
        if (!writer(arg, &size_word, sizeof(size_word))) {
            return tsf_false;
        }
        for (i = 0; i < num; ++i) {
            uint32_t type_code;
            if (!tsf_type_out_map_get_type_code(
                     type_map,
                     tsf_type_in_map_get_type(buf->types, i),
                     &type_code,
                     NULL)) {
                return tsf_false;
            }
            type_code = htonl(type_code);
            if (!writer(arg, &type_code, sizeof(type_code))) {
                return tsf_false;
            }
        }
    }
    
    switch (length_size) {
    case small_length: {
        uint8_t size_byte = buf->size;
        tsf_assert((uint32_t)size_byte == buf->size);
        if (!writer(arg, &size_byte, sizeof(size_byte))) {
            return tsf_false;
        }
        break;
    }
    case large_length: {
        uint32_t size_word = htonl(buf->size);
        if (!writer(arg, &size_word, sizeof(size_word))) {
            return tsf_false;
        }
        break;
    } }
    
    if (!writer(arg, buf->data, buf->size)) {
        return tsf_false;
    }
    
    return tsf_true;
}

tsf_bool_t tsf_buffer_write_simple(tsf_buffer_t *buf,
                                   uint32_t type_code,
                                   tsf_type_out_map_t *type_map,
                                   tsf_writer_t writer,
                                   void *arg) {
    type_code = htonl(type_code);
    
    if (!writer(arg, &type_code, sizeof(type_code))) {
        return tsf_false;
    }
    
    return write_buffer_impl(buf, type_map, writer, arg, large_length);
}

tsf_bool_t tsf_buffer_write_small_with_type_code(tsf_buffer_t *buf,
                                                 tsf_type_out_map_t *type_map,
                                                 tsf_writer_t writer,
                                                 void *arg) {
    return write_buffer_impl(buf, type_map, writer, arg, small_length);
}

tsf_bool_t tsf_buffer_write_to_buf(tsf_buffer_t *buf,
                                   tsf_type_out_map_t *type_map,
                                   uint8_t **target) {
    uint32_t type_code;
    
    if (!tsf_type_out_map_get_type_code(type_map,
                                        buf->type,
                                        &type_code,
                                        NULL)) {
        return tsf_false;
    }
    
    copy_htonl_incdst(*target,(uint8_t*)&type_code);

    if (tsf_type_is_dynamic(buf->type)) {
        uint32_t num,i;
        num=tsf_type_in_map_get_num_types(buf->types);
        copy_htonl_incdst(*target,(uint8_t*)&num);
        for (i=0;i<num;++i) {
            if (!tsf_type_out_map_get_type_code(
                     type_map,
                     tsf_type_in_map_get_type(buf->types,i),
                     &type_code,
                     NULL)) {
                return tsf_false;
            }
            copy_htonl_incdst(*target,(uint8_t*)&type_code);
        }
    }
    
    copy_htonl_incdst(*target,(uint8_t*)&buf->size);
    
    memcpy(*target,buf->data,buf->size);
    (*target)+=buf->size;
    
    return tsf_true;
}

tsf_bool_t tsf_buffer_write_whole(tsf_buffer_t *buf,
                                  tsf_writer_t writer,
                                  void *arg) {
    uint32_t size;
    
    if (buf==NULL) {
        return tsf_false;
    }
    
    if (!tsf_type_write(tsf_buffer_get_type(buf),
                        writer,
                        arg)) {
        return tsf_false;
    }
    
    if (tsf_type_is_dynamic(tsf_buffer_get_type(buf))) {
        uint32_t num,i;
        
        num=tsf_type_in_map_get_num_types(buf->types);
        size=htonl(num);
        if (!writer(arg,&size,sizeof(size))) {
            return tsf_false;
        }
        for (i=0;i<num;++i) {
            if (!tsf_type_write(tsf_type_in_map_get_type(buf->types,i),
                                writer,
                                arg)) {
                return tsf_false;
            }
        }
    }
    
    size=htonl(buf->size);
    if (!writer(arg,&size,sizeof(size))) {
        return tsf_false;
    }
    
    if (!writer(arg,buf->data,buf->size)) {
        return tsf_false;
    }
    
    return tsf_true;
}

tsf_buffer_t *tsf_buffer_read_whole(tsf_reader_t reader,
                                    void *arg,
                                    tsf_limits_t *limits) {
    tsf_buffer_t *ret;
    
    if (tsf_limits_max_types(limits)<1) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Limits do not allow for any types, while reading a buffer");
        return NULL;
    }
    
    ret=malloc(sizeof(tsf_buffer_t));
    if (ret==NULL) {
        tsf_set_errno("Could not malloc buffer in tsf_buffer_read_whole()");
        return NULL;
    }
    
    ret->keep=tsf_true;
    
    ret->type=tsf_type_read(reader,arg,limits);
    if (ret->type==NULL) {
        free(ret->type);
        return NULL;
    }
    
    if (tsf_type_is_dynamic(ret->type)) {
        uint32_t num_types;
        
        ret->types=tsf_type_in_map_create();
        if (ret->types==NULL) {
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
        
        if (!reader(arg,&num_types,sizeof(num_types))) {
            tsf_type_in_map_destroy(ret->types);
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
        
        num_types=ntohl(num_types);
        
        if (1+num_types > tsf_limits_max_types(limits)) {
            tsf_set_error(TSF_E_PARSE_ERROR,
                          "Max types limit exceeded.");
            tsf_type_in_map_destroy(ret->types);
            tsf_type_destroy(ret->type);
            free(ret);
            return NULL;
        }
        
        tsf_type_in_map_prepare(ret->types,num_types);
        
        while (num_types-->0) {
            if (!tsf_type_in_map_append(
                    ret->types,
                    tsf_type_read(reader,arg,limits))) {
                tsf_type_in_map_destroy(ret->types);
                tsf_type_destroy(ret->type);
                free(ret);
                return NULL;
            }
        }
    } else {
        ret->types=tsf_type_in_map_get_empty_singleton();
    }

    if (!reader(arg,&ret->size,sizeof(ret->size))) {
        tsf_type_in_map_destroy(ret->types);
        tsf_type_destroy(ret->type);
        free(ret);
        return NULL;
    }
    
    ret->size=ntohl(ret->size);
    
    if (ret->size>tsf_limits_max_buf_size(limits)) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "Buffer size exceeds limit");
        tsf_type_in_map_destroy(ret->types);
        tsf_type_destroy(ret->type);
        free(ret);
        return NULL;
    }
    
    ret->data=malloc(ret->size);
    if (ret->data==NULL) {
        tsf_set_errno("Could not allocate buffer data.");
        tsf_type_in_map_destroy(ret->types);
        tsf_type_destroy(ret->type);
        free(ret);
        return NULL;
    }
    
    if (!reader(arg,ret->data,ret->size)) {
        free(ret->data);
        tsf_type_in_map_destroy(ret->types);
        tsf_type_destroy(ret->type);
        free(ret);
        return NULL;
    }

    tsf_atomic_count_init(&ret->ref_count, 1);
    increment_buffer_balance();
    
    return ret;
}

tsf_bool_t tsf_buffer_calc_sha1(tsf_buffer_t *buf,
                                uint32_t *result) {
    tsf_sha1_wtr_t *sha1=tsf_sha1_writer_create();
    if (sha1==NULL) {
        return tsf_false;
    }
    
    if (!tsf_buffer_write_whole(buf,tsf_sha1_writer_write,sha1)) {
        tsf_sha1_writer_destroy(sha1);
        return tsf_false;
    }
    
    if (tsf_sha1_writer_finish(sha1)==NULL) {
        tsf_sha1_writer_destroy(sha1);
        return tsf_false;
    }
    
    memcpy(result,
           tsf_sha1_writer_result(sha1),
           sizeof(uint32_t)*5);
    
    tsf_sha1_writer_destroy(sha1);
    
    return tsf_true;
}

tsf_bool_t tsf_buffer_compare(tsf_buffer_t *a,
                              tsf_buffer_t *b) {
    if (a==b) {
        return tsf_true;
    }
    
    if (a->size!=b->size) {
        return tsf_false;
    }
    
    if (!tsf_type_compare(a->type,b->type)) {
        return tsf_false;
    }
    
    if (tsf_type_is_dynamic(a->type)) {
        tsf_assert(tsf_type_is_dynamic(b->type));
        tsf_assert(a->types!=NULL);
        tsf_assert(b->types!=NULL);
        
        if (!tsf_type_in_map_compare(a->types,b->types)) {
            return tsf_false;
        }
    } else {
        tsf_assert(a->types==tsf_type_in_map_get_empty_singleton());
        tsf_assert(b->types==tsf_type_in_map_get_empty_singleton());
    }
    
    if (memcmp(a->data,b->data,a->size)!=0) {
        return tsf_false;
    }
    
    return tsf_true;
}

tsf_type_t *tsf_buffer_get_type(tsf_buffer_t *buf) {
    return buf->type;
}

tsf_bool_t tsf_buffer_instanceof(tsf_buffer_t *buf, tsf_type_t *type) {
    return tsf_type_instanceof(tsf_buffer_get_type(buf), type);
}

tsf_type_in_map_t *tsf_buffer_get_shared_types(tsf_buffer_t *buf) {
    return buf->types;
}

