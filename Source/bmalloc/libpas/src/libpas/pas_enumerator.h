/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_ENUMERATOR_H
#define PAS_ENUMERATOR_H

#include "pas_allocation_config.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_enumerator;
struct pas_enumerator_region;
struct pas_heap_config;
struct pas_ptr_hash_set;
struct pas_root;
typedef struct pas_enumerator pas_enumerator;
typedef struct pas_enumerator_region pas_enumerator_region;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_ptr_hash_set pas_ptr_hash_set;
typedef struct pas_root pas_root;

enum pas_enumerator_record_kind {
    /* For meta-data that never contains objects. This is what malloc calls ADMIN_REGION_RANGE. */
    pas_enumerator_meta_record,

    /* For ranges of live and dead objects. This is what malloc calls PTR_REGION_RANGE. */
    pas_enumerator_payload_record,

    /* For individual live objects. This is what malloc calls PTR_IN_USE_RANGE. */
    pas_enumerator_object_record
};

typedef enum pas_enumerator_record_kind pas_enumerator_record_kind;

static inline const char* pas_enumerator_record_kind_get_string(pas_enumerator_record_kind kind)
{
    switch (kind) {
    case pas_enumerator_meta_record:
        return "meta";
    case pas_enumerator_payload_record:
        return "payload";
    case pas_enumerator_object_record:
        return "object";
    }
    PAS_ASSERT_WITH_DETAIL(!"Should not be reached");
    return NULL;
}

enum pas_enumerator_meta_recording_mode {
    pas_enumerator_do_not_record_meta_records,
    pas_enumerator_record_meta_records
};

typedef enum pas_enumerator_meta_recording_mode pas_enumerator_meta_recording_mode;

enum pas_enumerator_payload_recording_mode {
    pas_enumerator_do_not_record_payload_records,
    pas_enumerator_record_payload_records
};

typedef enum pas_enumerator_payload_recording_mode pas_enumerator_payload_recording_mode;

enum pas_enumerator_object_recording_mode {
    pas_enumerator_do_not_record_object_records,
    pas_enumerator_record_object_records
};

typedef enum pas_enumerator_object_recording_mode pas_enumerator_object_recording_mode;

typedef void* (*pas_enumerator_reader)(pas_enumerator* enumerator,
                                       void* remote_address, size_t size, void* arg);
typedef void (*pas_enumerator_recorder)(pas_enumerator* enumerator,
                                        void* remote_address, size_t size,
                                        pas_enumerator_record_kind kind,
                                        void* arg);

struct pas_enumerator {
    pas_enumerator_region* region;

    pas_allocation_config allocation_config;
    
    pas_root* root;

    void* compact_heap_remote_base;
    void* compact_heap_copy_base;
    size_t compact_heap_size;
    size_t compact_heap_guard_size;

    void** heap_config_datas; /* Indexed by pas_heap_config_kind. */

    char dummy_byte; /* If someone asks for a zero-byte read, we return this thing's address. */

    /* This starts out being populated by all pages that were ever allocated by the libpas instance
       and then has pages excluded from it (pas_enumerator_exclude_accounted_page). Before anything
       else, we exclude pages that are:
       
       - decommitted according to the large sharing pool and
       - reserved according to the payload reservation list.
    
       Later, large object enumeration considers each page that it thinks of as payload, and reports it
       as payload if it is still in this set, and then removes it from this set. */
    pas_ptr_hash_set* unaccounted_pages;

    pas_enumerator_reader reader;
    void* reader_arg;

    pas_enumerator_recorder recorder;
    void* recorder_arg;

    pas_enumerator_meta_recording_mode record_meta;
    pas_enumerator_payload_recording_mode record_payload;
    pas_enumerator_object_recording_mode record_object;
};

/* This thing has a goofy workflow - you basically have to do:
   
   1. pas_enumerator_create
   2. pas_enumerator_enumerate_all
   3. pas_enumerator_destroy
   
   FIXME: Reconsider exposing such API, consider instead just having a pas_enumerate_all() API that
   creates the enuemrator for you and then destroys it for you. */

PAS_API pas_enumerator* pas_enumerator_create(pas_root* remote_root_address,
                                              pas_enumerator_reader reader,
                                              void* reader_arg,
                                              pas_enumerator_recorder recorder,
                                              void* recorder_arg,
                                              pas_enumerator_meta_recording_mode record_meta,
                                              pas_enumerator_payload_recording_mode record_payload,
                                              pas_enumerator_object_recording_mode record_object);

PAS_API void pas_enumerator_destroy(pas_enumerator* enumerator);

PAS_API void* pas_enumerator_allocate(pas_enumerator* enumerator,
                                      size_t size);

PAS_API void* pas_enumerator_read_compact(pas_enumerator* enumerator,
                                          void* remote_address);

PAS_API void* pas_enumerator_read(pas_enumerator* enumerator,
                                  void* remote_address,
                                  size_t size);

PAS_API void pas_enumerator_add_unaccounted_pages(pas_enumerator* enumerator,
                                                  void* remote_address,
                                                  size_t size);

/* Returns true if the page was previously unaccounted. This is useful for large heap enumeration because
   in that case, if the page was missing then we know that it had been decommitted, and so is not actually
   part of the large payload. */
PAS_API bool pas_enumerator_exclude_accounted_page(pas_enumerator* enumerator,
                                                   void* remote_address);

PAS_API void pas_enumerator_exclude_accounted_pages(pas_enumerator* enumerator,
                                                    void* remote_address,
                                                    size_t size);

PAS_API void pas_enumerator_record(pas_enumerator* enumerator,
                                   void* remote_address,
                                   size_t size,
                                   pas_enumerator_record_kind kind);

PAS_API bool pas_enumerator_enumerate_all(pas_enumerator* enumerator);

PAS_END_EXTERN_C;

#endif /* PAS_ENUMERATOR_H */

