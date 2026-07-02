/*
 * Copyright (C) 2014 Filip Pizlo. All rights reserved.
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
#include "tsf_define_helpers.h"
#include <stdio.h>
#include <stdlib.h>

static void aoe_failure(void) {
    fprintf(stderr, "Error defining TSF types: %s\n", tsf_get_error());
    abort();
}

tsf_type_t *tsf_type_create_aoe(tsf_type_kind_t kind_code) {
    tsf_type_t *result = tsf_type_create(kind_code);
    if (result == NULL) {
        aoe_failure();
    }
    return result;
}

tsf_type_t *tsf_type_create_array_aoe(tsf_type_t *ele_type) {
    tsf_type_t *result = tsf_type_create_array(ele_type);
    if (result == NULL) {
        aoe_failure();
    }
    return result;
}

tsf_type_t *tsf_type_create_struct_with_native_size_aoe(size_t size) {
    tsf_type_t *result = tsf_type_create(TSF_TK_STRUCT);
    if (result == NULL) {
        aoe_failure();
    }
    if (!tsf_native_struct_type_set_size(&result, size)) {
        aoe_failure();
    }
    return result;
}

tsf_type_t *tsf_type_create_struct_with_native_size_and_destructor_aoe(size_t size, void (*dest)(void*)) {
    tsf_type_t *result = tsf_type_create_struct_with_native_size_aoe(size);
    if (!tsf_native_struct_type_set_destructor(&result, dest)) {
        aoe_failure();
    }
    return result;
}

tsf_type_t *tsf_type_create_choice_with_native_map_aoe(
    tsf_type_t **type, tsf_bool_t in_place, size_t value_offset, size_t data_offset, size_t size) {
    tsf_type_t *result = tsf_type_create(TSF_TK_CHOICE);
    if (result == NULL) {
        aoe_failure();
    }
    if (!tsf_native_choice_type_map(&result, in_place, value_offset, data_offset, size)) {
        aoe_failure();
    }
    return result;
}

void tsf_type_set_comment_aoe(tsf_type_t **type, const char *comment) {
    if (!tsf_type_set_comment(type, comment)) {
        aoe_failure();
    }
}

void tsf_struct_type_append_with_native_map_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *ele_type, size_t offset) {
    if (!tsf_struct_type_append(type, name, ele_type) ||
        !tsf_native_struct_type_map(type, name, offset)) {
        aoe_failure();
    }
}

void tsf_struct_type_append_primitive_with_native_map_aoe(
    tsf_type_t **type, const char *name, tsf_type_kind_t type_kind, size_t offset) {
    tsf_struct_type_append_with_native_map_aoe(
        type, name, tsf_type_create_aoe(type_kind), offset);
}

void tsf_struct_type_append_from_callback_and_dup_with_native_map_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *(*callback)(void), size_t offset) {
    tsf_struct_type_append_with_native_map_aoe(
        type, name, tsf_type_dup(callback()), offset);
}

void tsf_struct_type_append_with_native_map_with_comment_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *ele_type, size_t offset,
    const char *comment) {
    tsf_struct_type_append_with_native_map_aoe(type, name, ele_type, offset);
    if (!tsf_struct_type_set_comment(type, name, comment)) {
        aoe_failure();
    }
}

void tsf_struct_type_append_primitive_with_native_map_with_comment_aoe(
    tsf_type_t **type, const char *name, tsf_type_kind_t type_kind, size_t offset,
    const char *comment) {
    tsf_struct_type_append_with_native_map_with_comment_aoe(
        type, name, tsf_type_create_aoe(type_kind), offset, comment);
}

void tsf_struct_type_append_from_callback_and_dup_with_native_map_with_comment_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *(*callback)(void), size_t offset,
    const char *comment) {
    tsf_struct_type_append_with_native_map_with_comment_aoe(
        type, name, tsf_type_dup(callback()), offset, comment);
}

void tsf_choice_type_append_aoe(tsf_type_t **type, const char *name, tsf_type_t *ele_type) {
    if (!tsf_choice_type_append(type, name, ele_type)) {
        aoe_failure();
    }
}

void tsf_choice_type_append_primitive_aoe(
    tsf_type_t **type, const char *name, tsf_type_kind_t type_kind) {
    tsf_choice_type_append_aoe(type, name, tsf_type_create_aoe(type_kind));
}

void tsf_choice_type_append_from_callback_and_dup_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *(*callback)(void)) {
    tsf_choice_type_append_aoe(type, name, tsf_type_dup(callback()));
}

void tsf_choice_type_append_with_comment_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *ele_type, const char *comment) {
    tsf_choice_type_append_aoe(type, name, ele_type);
    if (!tsf_choice_type_set_comment(type, name, comment)) {
        aoe_failure();
    }
}

void tsf_choice_type_append_primitive_with_comment_aoe(
    tsf_type_t **type, const char *name, tsf_type_kind_t type_kind, const char *comment) {
    tsf_choice_type_append_with_comment_aoe(
        type, name, tsf_type_create_aoe(type_kind), comment);
}

void tsf_choice_type_append_from_callback_and_dup_with_comment_aoe(
    tsf_type_t **type, const char *name, tsf_type_t *(*callback)(void), const char *comment) {
    tsf_choice_type_append_with_comment_aoe(
        type, name, tsf_type_dup(callback()), comment);
}

void tsf_struct_type_set_optional_aoe(tsf_type_t **type, const char *name, tsf_bool_t optional) {
    if (!tsf_struct_type_set_optional(type, name, optional)) {
        aoe_failure();
    }
}

void tsf_type_set_name_aoe(tsf_type_t **type, const char *name) {
    if (!tsf_type_set_name(type, name)) {
        aoe_failure();
    }
}

void tsf_type_set_version_aoe(tsf_type_t **type, uint32_t major, uint32_t minor, uint32_t patch) {
    if (!tsf_type_set_version(type, major, minor, patch)) {
        aoe_failure();
    }
}

tsf_bool_t tsf_typed_data_write(tsf_stream_file_output_t *out, tsf_genrtr_t *generator, void *data) {
    tsf_buffer_t buf;
    char inlineBuffer[256];
    tsf_bool_t result;
    
    tsf_buffer_initialize_custom(&buf, inlineBuffer, sizeof(inlineBuffer));
    
    if (!tsf_generator_generate_existing_buffer(generator, data, &buf)) {
        return tsf_false;
    }
    
    result = tsf_stream_file_output_write(out, &buf);
    tsf_buffer_destroy_custom(&buf);
    return result;
}

void *tsf_typed_data_read(tsf_stream_file_input_t *in, tsf_parser_t *parser) {
    tsf_buffer_t buf;
    char inlineBuffer[256];
    void* result;
    
    tsf_buffer_initialize_custom(&buf, inlineBuffer, sizeof(inlineBuffer));
    
    if (!tsf_stream_file_input_read_existing_buffer(in, &buf)) {
        return NULL;
    }
    
    result = tsf_parser_parse(parser, &buf);
    tsf_buffer_destroy_custom(&buf);
    return result;
}

tsf_bool_t tsf_typed_data_read_into(tsf_stream_file_input_t *in, tsf_parser_t *parser, void *result) {
    tsf_buffer_t buf;
    char inlineBuffer[256];
    tsf_bool_t success;
    
    tsf_buffer_initialize_custom(&buf, inlineBuffer, sizeof(inlineBuffer));
    
    if (!tsf_stream_file_input_read_existing_buffer(in, &buf)) {
        return tsf_false;
    }
    
    success = tsf_parser_parse_into(parser, &buf, result, NULL);
    tsf_buffer_destroy_custom(&buf);
    return success;
}

