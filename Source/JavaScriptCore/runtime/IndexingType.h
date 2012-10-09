/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef IndexingType_h
#define IndexingType_h

#include <wtf/StdLibExtras.h>

namespace JSC {

typedef uint8_t IndexingType;

// Flags for testing the presence of capabilities.
static const IndexingType IsArray                  = 1;
static const IndexingType HasContiguous            = 2;
static const IndexingType HasArrayStorage          = 8;
static const IndexingType HasSlowPutArrayStorage   = 16;

// Additional flags for tracking the history of the type. These are usually
// masked off unless you ask for them directly.
static const IndexingType MayHaveIndexedAccessors  = 32;

// List of acceptable array types.
static const IndexingType NonArray                        = 0;
static const IndexingType NonArrayWithContiguous          = HasContiguous;
static const IndexingType NonArrayWithArrayStorage        = HasArrayStorage;
static const IndexingType NonArrayWithSlowPutArrayStorage = HasSlowPutArrayStorage;
static const IndexingType ArrayClass                      = IsArray; // I'd want to call this "Array" but this would lead to disastrous namespace pollution.
static const IndexingType ArrayWithContiguous             = IsArray | HasContiguous;
static const IndexingType ArrayWithArrayStorage           = IsArray | HasArrayStorage;
static const IndexingType ArrayWithSlowPutArrayStorage    = IsArray | HasSlowPutArrayStorage;

#define ALL_BLANK_INDEXING_TYPES \
    NonArray:                    \
    case ArrayClass

#define ALL_CONTIGUOUS_INDEXING_TYPES \
    NonArrayWithContiguous:           \
    case ArrayWithContiguous

#define ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES \
    ArrayWithArrayStorage:                      \
    case ArrayWithSlowPutArrayStorage
    
#define ALL_ARRAY_STORAGE_INDEXING_TYPES                \
    NonArrayWithArrayStorage:                           \
    case NonArrayWithSlowPutArrayStorage:               \
    case ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES

static inline bool hasIndexedProperties(IndexingType indexingType)
{
    switch (indexingType) {
    case ALL_BLANK_INDEXING_TYPES:
        return false;
    default:
        return true;
    }
}

static inline bool hasIndexingHeader(IndexingType type)
{
    return hasIndexedProperties(type);
}

static inline bool hasContiguous(IndexingType indexingType)
{
    return !!(indexingType & HasContiguous);
}

static inline bool hasArrayStorage(IndexingType indexingType)
{
    return !!(indexingType & (HasArrayStorage | HasSlowPutArrayStorage));
}

static inline bool shouldUseSlowPut(IndexingType indexingType)
{
    return !!(indexingType & HasSlowPutArrayStorage);
}

const char* indexingTypeToString(IndexingType);

// Mask of all possible types.
static const IndexingType AllArrayTypes            = 31;

// Mask of all possible types including the history.
static const IndexingType AllArrayTypesAndHistory  = 127;

} // namespace JSC

#endif // IndexingType_h

