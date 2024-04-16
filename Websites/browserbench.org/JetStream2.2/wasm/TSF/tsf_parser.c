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
#include "tsf_region.h"
#include "gpc.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

static int table_destroy_pair(tsf_type_t *type,
                              gpc_program_t *value,
                              void *arg) {
    tsf_type_destroy(type);
    gpc_program_destroy(value);
    return TSF_ST_CONTINUE;
}

tsf_parser_t *tsf_parser_create(tsf_type_t *type) {
    tsf_parser_t *result;
    
    if (!tsf_native_type_has_struct_mapping(type)) {
        tsf_set_error(TSF_E_NO_STRUCT_MAP, NULL);
        return NULL;
    }
    
    result = malloc(sizeof(tsf_parser_t));
    if (result == NULL) {
        tsf_set_errno("Could not malloc tsf_parser_t");
        return NULL;
    }
    
    result->parsers = tsf_st_init_ptrtable();
    if (result->parsers == NULL) {
        free(result);
        tsf_set_errno("Could not tsf_st_init_typetable()");
        return NULL;
    }
    
    result->expected_type = tsf_type_dup(type);
    result->expected_type_size = tsf_native_type_get_size(result->expected_type);
    
    result->cached_program = NULL;
    result->cached_type = NULL;

    tsf_mutex_init(&result->lock);
    
    return result;
}

void tsf_parser_destroy(tsf_parser_t *parser) {
    tsf_mutex_destroy(&parser->lock);
    tsf_st_foreach(parser->parsers,
                   (tsf_st_func_t)table_destroy_pair,
                   NULL);
    tsf_st_free_table(parser->parsers);
    tsf_type_destroy(parser->expected_type);
    free(parser);
}

uint32_t tsf_parser_num_codes(tsf_parser_t *parser) {
    return parser->parsers->num_entries;
}

static inline gpc_program_t *cached_program_lookup(tsf_parser_t *parser,
                                                   tsf_type_t *type) {
    if (parser->cached_type == type && parser->cached_program) {
        return parser->cached_program;
    } else {
        return NULL;
    }
}

static gpc_program_t *normal_program_lookup(tsf_parser_t *parser,
                                            tsf_type_t *type) {
    gpc_program_t *program;
    
    if (!tsf_st_lookup_ptrtable(parser->parsers,
                                type,
                                (void*)&program)) {
        /* not found, must create! */
        
        gpc_proto_t *proto =
            tsf_gpc_code_gen_generate_parser(parser->expected_type,
                                             type);
        
        if (proto == NULL) {
            return NULL;
        }
        
        gpc_code_gen_debug("parser", proto);
        
        program = gpc_program_from_proto(proto);
        
        gpc_proto_destroy(proto);
        
        if (program == NULL) {
            return NULL;
        }
        
        type = tsf_type_dup(type);
        
        if (tsf_st_insert(parser->parsers, (char*)type, program) < 0) {
            tsf_set_errno("Could not tsf_st_insert() in "
                          "tsf_parser_parse()");
            tsf_type_destroy(type);
            gpc_program_destroy(program);
            return NULL;
        }
            
        tsf_assert(!!parser->cached_type == !!parser->cached_program);
        if (!parser->cached_type) {
            parser->cached_type = type;
            parser->cached_program = program;
        }
    }
    
    return program;
}

tsf_bool_t tsf_parser_parse_into(tsf_parser_t *parser,
                                 tsf_buffer_t *buffer,
                                 void *result,
                                 void *region) {
    tsf_type_t *type;
    gpc_program_t *program;
    gpc_cell_t args[2];

    type = buffer->type;

    type = tsf_type_memo(type);
    if (!type) {
        return tsf_false;
    }
    
#ifdef TSF_HAVE_FENCES
    program = cached_program_lookup(parser, type);
    if (program != NULL) {
        tsf_basic_fence();
    } else {
        tsf_mutex_lock(&parser->lock);
        program = normal_program_lookup(parser, type);
        tsf_mutex_unlock(&parser->lock);
        if (program == NULL) {
            return tsf_false;
        }
    }
#else
    tsf_mutex_lock(&parser->lock);
    program = cached_program_lookup(parser, type);
    if (program == NULL) {
        program = normal_program_lookup(parser, type);
    }
    tsf_mutex_unlock(&parser->lock);
    if (program == NULL) {
        return tsf_false;
    }
#endif
    
    args[0] = (gpc_cell_t)result;
    args[1] = (gpc_cell_t)buffer;
    
    return (tsf_bool_t)gpc_program_run(program, region, args);
}

void *tsf_parser_parse(tsf_parser_t *parser,
                       tsf_buffer_t *buffer) {
    void *result = tsf_region_create(parser->expected_type_size);
    if (!result) {
        tsf_set_errno("Could not tsf_region_create()");
        return NULL;
    }
    if (!tsf_parser_parse_into(parser, buffer, result, result)) {
        tsf_region_free(result);
        return NULL;
    }
    return result;
}

void *tsf_parser_parse_in(tsf_parser_t *parser,
                          tsf_buffer_t *buffer,
                          void *region) {
    void *result;
    if (region) {
        result = tsf_region_alloc(region, parser->expected_type_size);
    } else {
        region = result = tsf_region_create(parser->expected_type_size);
    }
    if (!result) {
        tsf_set_errno("Could not tsf_region_alloc()");
        return NULL;
    }
    if (!tsf_parser_parse_into(parser, buffer, result, region)) {
        if (result == region) {
            tsf_region_free(region);
        }
        return NULL;
    }
    return result;
}

