/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CompactRefPtr.h>

namespace JSC {

class CompactBloomFilter {
    using Compacted = WTF::Compacted<nullptr_t>;
    using StorageSize = Compacted::StorageSize;

public:
    CompactBloomFilter() = default;

    CompactBloomFilter(StorageSize bits)
        : m_bits(bits)
    {
    }

    void add(uintptr_t ptr) { m_bits |= Compacted::encode(ptr); }

    void add(CompactBloomFilter& other) { m_bits |= other.m_bits; }

    bool ruleOut(uintptr_t ptr) const
    {
        StorageSize bits = Compacted::encode(ptr);
        if (!bits)
            return true;

        if ((bits & m_bits) != bits)
            return true;

        return false;
    } // True for 0.

    void reset() { m_bits = 0; }

    StorageSize bits() const { return m_bits; }

private:
    StorageSize m_bits { 0 };
};

} // namespace JSC
