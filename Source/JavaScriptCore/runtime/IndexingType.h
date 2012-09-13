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

namespace JSC {

typedef uint8_t IndexingType;

// Flags for testing the presence of capabilities.
static const IndexingType IsArray                  = 1;
static const IndexingType HasArrayStorage          = 8;

// Additional flags for tracking the history of the type. These are usually
// masked off unless you ask for them directly.
static const IndexingType HadArrayStorage          = 16; // Means that this object did have array storage in the past.

// List of acceptable array types.
static const IndexingType NonArray                 = 0;
static const IndexingType NonArrayWithArrayStorage = HasArrayStorage;
static const IndexingType ArrayClass               = IsArray; // I'd want to call this "Array" but this would lead to disastrous namespace pollution.
static const IndexingType ArrayWithArrayStorage    = IsArray | HasArrayStorage;

// Mask of all possible types.
static const IndexingType AllArrayTypes            = 15;

// Mask of all possible types including the history.
static const IndexingType AllArrayTypesAndHistory  = 31;

inline bool hasIndexingHeader(IndexingType type)
{
    return !!(type & HasArrayStorage);
}

} // namespace JSC

#endif // IndexingType_h

