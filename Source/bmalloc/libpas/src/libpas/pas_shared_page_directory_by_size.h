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

#ifndef PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_H
#define PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_H

#include "pas_page_sharing_mode.h"
#include "pas_segregated_shared_page_directory.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_page_config;
struct pas_shared_page_directory_by_size;
struct pas_shared_page_directory_by_size_data;
typedef struct pas_segregated_page_config pas_segregated_page_config;
typedef struct pas_shared_page_directory_by_size pas_shared_page_directory_by_size;
typedef struct pas_shared_page_directory_by_size_data pas_shared_page_directory_by_size_data;

struct pas_shared_page_directory_by_size {
    /* These is a configuration parameter, which you may change before this gets first used. If
       you set it after data is not NULL, then nothing happens. */
    unsigned log_shift;
    pas_page_sharing_mode sharing_mode;

    pas_shared_page_directory_by_size_data* data;
};

struct pas_shared_page_directory_by_size_data {
    /* These are the actual settings that are being used. */
    unsigned log_shift;
    unsigned num_directories;

    pas_segregated_shared_page_directory directories[1];
};

#define PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_INITIALIZER(passed_log_shift, passed_sharing_mode) \
    ((pas_shared_page_directory_by_size){ \
         .log_shift = (passed_log_shift), \
         .sharing_mode = (passed_sharing_mode), \
         .data = NULL \
     })

PAS_API pas_segregated_shared_page_directory* pas_shared_page_directory_by_size_get(
    pas_shared_page_directory_by_size* by_size,
    unsigned size,
    pas_segregated_page_config* page_config);

PAS_API bool pas_shared_page_directory_by_size_for_each(
    pas_shared_page_directory_by_size* by_size,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);

PAS_API bool pas_shared_page_directory_by_size_for_each_remote(
    pas_shared_page_directory_by_size* by_size,
    pas_enumerator* enumerator,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);

PAS_API void pas_shared_page_directory_by_size_dump_directory_arg(
    pas_stream* stream,
    pas_segregated_shared_page_directory* directory);

PAS_END_EXTERN_C;

#endif /* PAS_SHARED_PAGE_DIRECTORY_BY_SIZE_H */

