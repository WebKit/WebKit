/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DoubleToStringCache.h"

#include "JSCInlines.h"
#include <wtf/Assertions.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/dragonbox/dragonbox_to_chars.h>
#include <wtf/dtoa.h>
#include <wtf/dtoa/double-conversion.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DoubleToStringCache);

const String& DoubleToStringCache::add(VM& vm, double d)
{
    uint64_t bits = std::bit_cast<uint64_t>(d);
    if (!bits)
        return vm.numericStrings.add(0);

    auto& entry = acquire(bits);
    if (bits == entry.key)
        return entry.value;
    entry.key = bits;
    entry.value = String::number(d);
    entry.jsString = nullptr;
    return entry.value;
}

JSString* DoubleToStringCache::addJSString(VM& vm, double value)
{
    uint64_t bits = std::bit_cast<uint64_t>(value);
    if (!bits)
        return vm.numericStrings.addJSString(vm, 0);

    auto& entry = acquire(bits);
    if (value != entry.key) {
        entry.key = bits;
        entry.value = String::number(value);
    } else {
        if (entry.jsString)
            return entry.jsString;
    }
    entry.jsString = jsNontrivialString(vm, entry.value);
    return entry.jsString;
}

ALWAYS_INLINE unsigned DoubleToStringCache::primaryHash(uint64_t bits)
{
    return static_cast<uint32_t>(bits) ^ static_cast<uint32_t>(bits >> 32);
}

ALWAYS_INLINE unsigned DoubleToStringCache::secondaryHash(uint64_t bits)
{
    return WTF::IntHash<uint64_t>::hash(bits);
}

ALWAYS_INLINE DoubleToStringCache::CacheEntryWithJSString<uint64_t>& DoubleToStringCache::acquire(uint64_t bits)
{
    auto& entry0 = m_primaryCache[primaryHash(bits) & (m_primaryCache.size() - 1)];
    if (entry0.key == bits)
        return entry0;

    auto& entry1 = m_secondaryCache[secondaryHash(bits) & (m_secondaryCache.size() - 1)];
    if (entry1.key == bits)
        return entry1;

    if (entry0.key)
        m_secondaryCache[secondaryHash(entry0.key) & (m_secondaryCache.size() - 1)] = std::exchange(entry0, { });
    return entry0;
}

template<typename Visitor>
void DoubleToStringCache::visitAggregateImpl(Visitor& visitor)
{
    if (!(visitor.heap()->collectionScope() == CollectionScope::Full)) {
        for (auto& entry : m_primaryCache)
            visitor.appendUnbarriered(entry.jsString);
        for (auto& entry : m_secondaryCache)
            visitor.appendUnbarriered(entry.jsString);
    }
}

DEFINE_VISIT_AGGREGATE(DoubleToStringCache);

void DoubleToStringCache::finalizeUnconditionally(VM& vm)
{
    for (auto& entry : m_primaryCache) {
        if (entry.jsString && !vm.heap.isMarked(entry.jsString))
            entry.jsString = nullptr;
    }
    for (auto& entry : m_secondaryCache)
        if (entry.jsString && !vm.heap.isMarked(entry.jsString))
            entry.jsString = nullptr;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
