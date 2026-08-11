/*
 * Copyright (C) 2004, 2005, 2015 Filip Pizlo. All rights reserved.
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

tsf_named_type_t *tsf_named_type_create(const char *name,
                                        tsf_type_t *type,
                                        uint32_t index) {
    tsf_named_type_t *node=malloc(sizeof(tsf_named_type_t));
    if (node==NULL) {
        tsf_set_errno("Could not malloc tsf_named_type_t");
        goto failure_1;
    }
    
    if (strlen(name)+1>TSF_STR_SIZE) {
        tsf_set_error(TSF_E_STR_OVERFLOW,
                      "String '%s' is too long to be used as an element "
                      "name",name);
        goto failure_2;
    }
    
    node->name=strdup(name);
    if (node->name==NULL) {
        tsf_set_errno("Could not allocate name");
        goto failure_2;
    }
    
    node->type=type;
    node->optional=tsf_false;
    node->has_offset=tsf_false;
    node->offset=0;
    node->index=index;
    node->comment=NULL;
    
    return node;
    
failure_2:
    free(node);
failure_1:
    tsf_type_destroy(type);
    return NULL;
}

void tsf_named_type_destroy(tsf_named_type_t *node) {
    tsf_type_destroy(node->type);
    free(node->name);
    if (node->comment!=NULL) {
        free(node->comment);
    }
    free(node);
}

const char *tsf_named_type_get_name(tsf_named_type_t *node) {
    return node->name;
}

tsf_type_t *tsf_named_type_get_type(tsf_named_type_t *node) {
    return node->type;
}

uint32_t tsf_named_type_get_index(tsf_named_type_t *node) {
    return node->index;
}

tsf_bool_t tsf_named_type_get_optional(tsf_named_type_t *node) {
    return node->optional;
}

void tsf_named_type_set_optional(tsf_named_type_t *node,
                                 tsf_bool_t optional) {
    node->optional = optional;
}

const char *tsf_named_type_get_comment(tsf_named_type_t *node) {
    return node->comment;
}

tsf_bool_t tsf_named_type_has_comment(tsf_named_type_t *node) {
    return node->comment != NULL;
}

tsf_bool_t tsf_named_type_set_comment(tsf_named_type_t *node,
                                      const char *comment) {
    char *comment_copy;
    
    if (strlen(comment)+1>TSF_MAX_COMMENT_SIZE) {
        tsf_set_error(TSF_E_STR_OVERFLOW,
                      "Comment string too long.");
        return tsf_false;
    }
    
    comment_copy=strdup(comment);
    if (comment_copy==NULL) {
        tsf_set_errno("Could not strcpy comment");
        return tsf_false;
    }
    free(node->comment);
    node->comment=comment_copy;
    
    return tsf_true;
}

int tsf_named_type_get_hash(tsf_named_type_t *node) {
    return tsf_st_strhash(node->name)|
           (tsf_type_get_hash(node->type)<<8)|
	   node->optional<<16;
}



