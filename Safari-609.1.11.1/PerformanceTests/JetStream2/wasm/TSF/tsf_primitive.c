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

/* FIXME: primitive type should really just all be instances of each other. */
tsf_bool_t tsf_primitive_type_kind_instanceof(tsf_type_kind_t one,
                                              tsf_type_kind_t two) {
    return (tsf_primitive_type_kind_lower_bound(one)
            >= tsf_primitive_type_kind_lower_bound(two))
	&& (tsf_primitive_type_kind_upper_bound(one)
            <= tsf_primitive_type_kind_upper_bound(two))
	&& (tsf_primitive_type_kind_precision(one)
            <= tsf_primitive_type_kind_precision(two));
}

#define test_type_kind(kind_code) \
    if (lower_bound >= tsf_primitive_type_kind_lower_bound(kind_code) && \
	upper_bound <= tsf_primitive_type_kind_upper_bound(kind_code) && \
	precision <= tsf_primitive_type_kind_precision(kind_code)) { \
	return kind_code; \
    }

tsf_type_kind_t tsf_primitive_type_kind_for_spec(int32_t lower_bound,
						 int32_t upper_bound,
						 int32_t precision) {
    test_type_kind(TSF_TK_INT8);
    test_type_kind(TSF_TK_UINT8);
    test_type_kind(TSF_TK_INT16);
    test_type_kind(TSF_TK_UINT16);
    test_type_kind(TSF_TK_INT32);
    test_type_kind(TSF_TK_UINT32);
    test_type_kind(TSF_TK_INT64);
    test_type_kind(TSF_TK_UINT64);
    test_type_kind(TSF_TK_FLOAT);
    return TSF_TK_DOUBLE;
}

tsf_type_kind_t tsf_primitive_type_kind_mix(tsf_type_kind_t one,
					    tsf_type_kind_t two) {
    return tsf_primitive_type_kind_for_spec
	(tsf_min(tsf_primitive_type_kind_lower_bound(one),
		 tsf_primitive_type_kind_lower_bound(two)),
	 tsf_max(tsf_primitive_type_kind_upper_bound(one),
		 tsf_primitive_type_kind_upper_bound(two)),
	 tsf_max(tsf_primitive_type_kind_precision(one),
		 tsf_primitive_type_kind_precision(two)));
}





