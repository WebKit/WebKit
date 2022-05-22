/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "TinyBloomFilter.h"
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

struct GetByValHistory {
    void observeNonUID()
    {
        uintptr_t count = Options::getByValICMaxNumberOfIdentifiers() + 1;
        update(count, filter());
    }

    void observe(const UniquedStringImpl* impl)
    {
        if (!impl) {
            observeNonUID();
            return;
        }

        uintptr_t count = this->count();
        uintptr_t filter = this->filter();

        TinyBloomFilter bloomFilter(filter);
        uintptr_t implBits = bitwise_cast<uintptr_t>(impl);
        ASSERT(((static_cast<uint64_t>(implBits) << 8) >> 8) == static_cast<uint64_t>(implBits));
        if (bloomFilter.ruleOut(implBits)) {
            bloomFilter.add(implBits);
            ++count;
            update(count, bloomFilter.bits());
        }
    }

    uintptr_t count() const { return static_cast<uintptr_t>(m_payload >> 56); }

private:
    uintptr_t filter() const { return static_cast<uintptr_t>((m_payload << 8) >> 8); }

    void update(uint64_t count, uint64_t filter)
    {
        ASSERT(((filter << 8) >> 8) == filter);
        m_payload = (count << 56) | filter;
    }

    uint64_t m_payload { 0 };
};

} // namespace JSC
