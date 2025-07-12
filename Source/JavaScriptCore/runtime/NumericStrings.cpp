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
#include "NumericStrings.h"

#include "JSString.h"
#include "VM.h"
#include <wtf/Assertions.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

template<typename Visitor>
void NumericStrings::visitAggregateImpl(Visitor& visitor)
{
    for (auto& entry : m_intCache)
        visitor.appendUnbarriered(entry.jsString);
    // 0-9 are managed by SmallStrings. They never die.
    for (unsigned i = 10; i < m_smallIntCache.size(); ++i)
        visitor.appendUnbarriered(m_smallIntCache[i].jsString);
    if (m_doubleCache)
        m_doubleCache->visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(NumericStrings);

void NumericStrings::finalizeUnconditionally(VM& vm, CollectionScope collectionScope)
{
    if (collectionScope == CollectionScope::Full) {
        for (auto& entry : m_intCache)
            entry.jsString = nullptr;
        // 0-9 are managed by SmallStrings. They never die.
        for (unsigned i = 10; i < m_smallIntCache.size(); ++i)
            m_smallIntCache[i].jsString = nullptr;
    }
    if (m_doubleCache)
        m_doubleCache->finalizeUnconditionally(vm, collectionScope);
}

void NumericStrings::doubleToStringCacheSlow()
{
    auto cache = makeUnique<DoubleToStringCache>();
    WTF::storeStoreFence();
    m_doubleCache = WTFMove(cache);
}

void NumericStrings::initializeSmallIntCache(VM& vm)
{
    for (int i = 0; i < 10; ++i) {
        auto* string = vm.smallStrings.singleCharacterString(i + '0');
        auto& entry = lookupSmallString(static_cast<unsigned>(i));
        entry.jsString = string;
        ASSERT(string->tryGetValueImpl());
        entry.value = string->tryGetValue();
    }
}

JSString* NumericStrings::addJSString(VM& vm, int i)
{
    if (static_cast<unsigned>(i) < cacheSize) {
        auto& entry = lookupSmallString(static_cast<unsigned>(i));
        if (entry.jsString)
            return entry.jsString;
        entry.jsString = jsNontrivialString(vm, entry.value);
        return entry.jsString;
    }
    auto& entry = lookup(i);
    if (i != entry.key || entry.value.isNull()) {
        entry.key = i;
        entry.value = String::number(i);
    } else {
        if (entry.jsString)
            return entry.jsString;
    }
    entry.jsString = jsNontrivialString(vm, entry.value);
    return entry.jsString;
}

JSString* NumericStrings::addJSString(VM& vm, double value)
{
    return doubleToStringCache().addJSString(vm, value);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
