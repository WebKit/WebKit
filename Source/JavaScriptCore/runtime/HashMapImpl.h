/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionHelpers.h"
#include "JSCJSValue.h"
#include "JSObject.h"

namespace JSC {

enum class HashTableType {
    Key,
    KeyValue
};

struct HashMapBucketDataKey {
    static const HashTableType Type = HashTableType::Key;
    WriteBarrier<Unknown> key;
};

struct HashMapBucketDataKeyValue {
    static const HashTableType Type = HashTableType::KeyValue;
    WriteBarrier<Unknown> key;
    WriteBarrier<Unknown> value;
};



ALWAYS_INLINE bool areKeysEqual(JSGlobalObject*, JSValue, JSValue);

// Note that normalization is inlined in DFG's NormalizeMapKey.
// Keep in sync with the implementation of DFG and FTL normalization.
ALWAYS_INLINE JSValue normalizeMapKey(JSValue key);
ALWAYS_INLINE uint32_t wangsInt64Hash(uint64_t key);
ALWAYS_INLINE uint32_t jsMapHash(JSBigInt*);
ALWAYS_INLINE uint32_t jsMapHash(JSGlobalObject*, VM&, JSValue);
ALWAYS_INLINE uint32_t shouldShrink(uint32_t capacity, uint32_t keyCount);
ALWAYS_INLINE uint32_t shouldRehash(uint32_t capacity, uint32_t keyCount, uint32_t deleteCount);
ALWAYS_INLINE uint32_t nextCapacity(uint32_t capacity, uint32_t keyCount);

} // namespace JSC
