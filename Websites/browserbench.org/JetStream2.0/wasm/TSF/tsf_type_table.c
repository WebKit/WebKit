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
#include "tsf_format.h"

#include <string.h>

tsf_type_table_t *tsf_type_table_create() {
    tsf_type_table_t *ret = malloc(sizeof(tsf_type_table_t));
    if (ret == NULL) {
        tsf_set_errno("Could not malloc tsf_type_table_t");
        return NULL;
    }
    
    ret->elements = NULL;
    ret->num_elements = 0;
    ret->ele_table = tsf_st_init_strtable();
    if (ret->ele_table == NULL) {
        tsf_set_errno("Could not tsf_st_init_strtable()");
        free(ret);
        return NULL;
    }
    
    return ret;
}

tsf_bool_t tsf_type_table_copy(tsf_type_table_t *dest,
                               tsf_type_table_t *src) {
    uint32_t i;
    
    for (i = 0; i < tsf_type_table_get_num_elements(src); ++i) {
        tsf_named_type_t *n = tsf_type_table_find_by_index(src, i), *n2;
        tsf_type_t *new_type = tsf_type_dup(n->type);
        if (new_type == NULL) {
            return tsf_false;
        }
        if (!tsf_type_table_append(dest, n->name, new_type)) {
            return tsf_false;
        }
        
        /* copy the optional attribute, offset info manually */
        n2 = tsf_type_table_find_by_index(dest,
                                          tsf_type_table_get_num_elements(dest) - 1);
	n2->optional = n->optional;
        if (n->has_offset) {
            n2->has_offset = tsf_true;
            n2->offset = n->offset;
        }
        
        /* copy the comment manually */
        if (n->comment != NULL) {
            n2->comment = strdup(n->comment);
            if (n2->comment == NULL) {
                tsf_set_errno("Could not allocate comment");
                return tsf_false;
            }
        }
    }
    
    return tsf_true;
}

void tsf_type_table_destroy(tsf_type_table_t *tt) {
    uint32_t i;
    for (i = 0; i < tt->num_elements; ++i) {
        tsf_named_type_destroy(tt->elements[i]);
    }
    if (tt->elements != NULL) {
        free(tt->elements);
    }
    tsf_st_free_table(tt->ele_table);
    free(tt);
}

tsf_type_table_t *tsf_type_table_read(const char *name,
                                      tsf_reader_t reader,
                                      void *arg,
                                      tsf_limits_t *limits,
                                      uint32_t size_limit,
                                      uint32_t depth) {
    tsf_type_table_t *ret = tsf_type_table_create();
    uint32_t i, num;
    
    if (ret == NULL) {
        return NULL;
    }
    
    if (!reader(arg, &num, sizeof(num))) {
        tsf_type_table_destroy(ret);
        return NULL;
    }
    
    num = ntohl(num);
    
    if (num > size_limit) {
        tsf_set_error(TSF_E_PARSE_ERROR,
                      "%s size exceeds limit while parsing", name);
        tsf_type_table_destroy(ret);
        return NULL;
    }
    
    for (i = 0; i < num; ++i) {
        uint8_t ui8_tmp;
        char *str;
        
        /* parse the string */
        str = tsf_read_string(reader, arg, TSF_STR_SIZE);
        if (str == NULL) {
            tsf_type_table_destroy(ret);
            return NULL;
        }
        
        /* now do other things */
        if (!tsf_type_table_append(ret,
                                   str,
                                   tsf_type_read_rec(reader,
                                                     arg,
                                                     limits,
                                                     depth))) {
            tsf_type_table_destroy(ret);
            free(str);
            return NULL;
        }
        
        free(str);
        
        if (!reader(arg, &ui8_tmp, 1)) {
            tsf_type_table_destroy(ret);
            return NULL;
        }
	
	if (ui8_tmp & ~3) {
	    tsf_set_error(TSF_E_PARSE_ERROR,
			  "Flags field contains bits I don't recognize: " fui8,
			  ui8_tmp);
	    tsf_type_table_destroy(ret);
	    return NULL;
	}
	
	ret->elements[ret->num_elements - 1]->optional =
	    (ui8_tmp & 2 ? tsf_true : tsf_false);
        
        if (ui8_tmp & 1) {
            /* parse a comment string.  it can be arbitrarily long, so
               this parsing code is somewhat of a pain in the ass to
               write. */
            
            if (!tsf_limits_allow_comments(limits)) {
                tsf_set_error(TSF_E_PARSE_ERROR,
                              "Encountered a commented type while parsing, "
                              "but comments are not allowed");
                tsf_type_table_destroy(ret);
                return NULL;
            }
            
            str = tsf_read_string(reader, arg, TSF_MAX_COMMENT_SIZE);
            if (str == NULL) {
                tsf_type_table_destroy(ret);
                return NULL;
            }
            
            ret->elements[ret->num_elements - 1]->comment = str;
        }
    }
    
    return ret;
}

tsf_bool_t tsf_type_table_write(tsf_type_table_t *tt,
                                tsf_writer_t writer,
                                void *arg) {
    uint32_t ui32_tmp = htonl(tt->num_elements);
    uint8_t ui8_tmp;
    uint32_t i;
    if (!writer(arg, &ui32_tmp, sizeof(ui32_tmp))) {
        return tsf_false;
    }
    
    for (i = 0; i < tt->num_elements; ++i) {
        tsf_named_type_t *n = tt->elements[i];
        
        if (!tsf_write_string(writer, arg, tsf_named_type_get_name(n))) {
            return tsf_false;
        }
        
        if (!tsf_type_write(tsf_named_type_get_type(n),
                            writer, arg)) {
            return tsf_false;
        }
	
	ui8_tmp = 0;
	if (n->comment != NULL) {
	    ui8_tmp |= 1;
	}
	if (n->optional) {
	    ui8_tmp |= 2;
	}
	if (!writer(arg, &ui8_tmp, 1)) {
	    return tsf_false;
	}
	
        if (n->comment != NULL) {
            if (!tsf_write_string(writer, arg, n->comment)) {
                return tsf_false;
            }
        }
    }
    
    return tsf_true;
}

int tsf_type_table_get_hash(tsf_type_table_t *tt) {
    int ret = 0;
    uint32_t i;
    for (i = 0; i < tt->num_elements; ++i) {
        ret += tsf_named_type_get_hash(tt->elements[i]);
    }
    return ret;
}

tsf_bool_t tsf_type_table_contains_dynamic(tsf_type_table_t *tt) {
    uint32_t i;
    for (i = 0; i < tt->num_elements; ++i) {
        if (tsf_type_is_dynamic(tt->elements[i]->type)) {
            return tsf_true;
        }
    }
    return tsf_false;
}

tsf_bool_t tsf_type_table_compare(tsf_type_table_t *a,
                                  tsf_type_table_t *b) {
    uint32_t i;
    
    if (a->num_elements != b->num_elements) {
        return tsf_false;
    }
    
    for (i = 0; i < a->num_elements; ++i) {
        if (strcmp(a->elements[i]->name,
                   b->elements[i]->name)) {
            return tsf_false;
        }
	
	if (a->elements[i]->optional
	    != b->elements[i]->optional) {
	    return tsf_false;
	}
        
        if (!tsf_type_compare(a->elements[i]->type,
                              b->elements[i]->type)) {
            return tsf_false;
        }
    }
    
    return tsf_true;
}

tsf_bool_t tsf_type_table_append(tsf_type_table_t *tt,
                                 const char *name,
                                 tsf_type_t *ele_type) {
    tsf_named_type_t *n;
    
    if (ele_type == NULL) {
        return tsf_false;
    }
    
    if (tsf_st_lookup(tt->ele_table, (char*)name, NULL)) {
        tsf_set_error(TSF_E_NAME_COLLISION,
                      "Name '%s' is already used by another element",
                      name);
        tsf_type_destroy(ele_type);
        return tsf_false;
    }
    
    if (tt->elements == NULL) {
        tt->elements = malloc(sizeof(tsf_named_type_t*));
        if (tt->elements == NULL) {
            tsf_set_errno("Could not malloc tsf_named_type_t*");
            tsf_type_destroy(ele_type);
            return tsf_false;
        }
    } else {
        tsf_named_type_t **new_array = realloc(tt->elements,
                                               sizeof(tsf_named_type_t*) *
                                               (tt->num_elements + 1));
        if (new_array==NULL) {
            tsf_set_errno("Could not really array of tsf_named_type_t*");
            tsf_type_destroy(ele_type);
            return tsf_false;
        }
        tt->elements = new_array;
    }
    
    n = tsf_named_type_create(name,
                              ele_type,
                              tt->num_elements);
    if (n == NULL) {
        return tsf_false;
    }
    
    if (tsf_st_insert(tt->ele_table, n->name, n) < 0) {
        tsf_set_errno("Failed to tsf_st_insert()");
        tsf_type_destroy(ele_type);
        free(n);
        return tsf_false;
    }
    
    tt->elements[tt->num_elements++] = n;
    
    return tsf_true;
}

tsf_bool_t tsf_type_table_remove(tsf_type_table_t *tt,
                                 const char *name) {
    char *to_delete;
    void *old_value;
    tsf_named_type_t *n;
    
    to_delete = (char*)name;
    old_value = NULL;
    tsf_st_delete(tt->ele_table, &to_delete, &old_value);
    if (old_value == NULL) {
        tsf_set_error(TSF_E_ELE_NOT_FOUND, "Could not find \'%s\'", name);
        return tsf_false;
    }
    
    n = (tsf_named_type_t*)old_value;
    tt->elements[n->index] = tt->elements[--tt->num_elements];
    tsf_named_type_destroy(n);
    return tsf_true;
}

tsf_named_type_t *tsf_type_table_find_by_name(tsf_type_table_t *tt,
                                              const char *name) {
    tsf_named_type_t *n;
    if (!tsf_st_lookup(tt->ele_table, (char*)name, (void**)&n)) {
        tsf_set_error(TSF_E_ELE_NOT_FOUND, "Could not find \'%s\'", name);
        return tsf_false;
    }
    
    return n;
}

tsf_named_type_t *tsf_type_table_find_by_index(tsf_type_table_t *tt,
                                               uint32_t index) {
    return tt->elements[index];
}

uint32_t tsf_type_table_get_num_elements(tsf_type_table_t *tt) {
    return tt->num_elements;
}


