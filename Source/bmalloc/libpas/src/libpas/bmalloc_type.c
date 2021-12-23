/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "bmalloc_type.h"

#include "pas_immortal_heap.h"
#include "pas_stream.h"
#include <string.h>

bmalloc_type* bmalloc_type_create(size_t size, size_t alignment, const char* name)
{
    bmalloc_type* result;

    PAS_ASSERT((unsigned)size == size);
    PAS_ASSERT((unsigned)alignment == alignment);

    result = pas_immortal_heap_allocate(
        sizeof(bmalloc_type),
        "bmalloc_type",
        pas_object_allocation);

    *result = BMALLOC_TYPE_INITIALIZER((unsigned)size, (unsigned)alignment, name);

    return result;
}

bool bmalloc_type_try_name_dump(pas_stream* stream, const char* name)
{
    const char* type_name_start_marker = "[LibPasBmallocHeapType = ";
    
    char* type_name_start_marker_ptr;
    char* type_name_start_ptr;
    unsigned bracket_balance;
    size_t index;

    type_name_start_marker_ptr = strstr(name, type_name_start_marker);
    if (!type_name_start_marker_ptr)
        return false;

    type_name_start_ptr = type_name_start_marker_ptr + strlen(type_name_start_marker);

    bracket_balance = 0;

    for (index = 0; type_name_start_ptr[index]; ++index) {
        switch (type_name_start_ptr[index]) {
        case '[':
            bracket_balance++;
            break;
        case ']':
            if (!bracket_balance) {
                char* flexible_array_member_marker;

                flexible_array_member_marker = strstr(name, "primitiveHeapRefForTypeWithFlexibleArrayMember");
                if (flexible_array_member_marker)
                    pas_stream_printf(stream, "ObjectWithFlexibleArrayMember, ");

                PAS_ASSERT((size_t)(int)index == index);
                pas_stream_printf(stream, "%.*s", (int)index, type_name_start_ptr);
                return true;
            }
            bracket_balance--;
            break;
        default:
            break;
        }
    }

    return false;
}

void bmalloc_type_name_dump(pas_stream* stream, const char* name)
{
    if (bmalloc_type_try_name_dump(stream, name))
        return;

    pas_stream_printf(stream, "%s", name);
}

void bmalloc_type_dump(const bmalloc_type* type, pas_stream* stream)
{
    pas_stream_printf(
        stream, "Size = %zu, Alignment = %zu, Type = ",
        bmalloc_type_size(type), bmalloc_type_alignment(type));

    bmalloc_type_name_dump(stream, bmalloc_type_name(type));
}

void bmalloc_type_as_heap_type_dump(const pas_heap_type* type, pas_stream* stream)
{
    bmalloc_type_dump((const bmalloc_type*)type, stream);
}

#endif /* LIBPAS_ENABLED */

