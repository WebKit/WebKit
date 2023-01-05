/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
#include "CSSCounterStyleRegistry.h"

#include "CSSCounterStyle.h"
#include "CSSCounterStyleRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"

namespace WebCore {

void CSSCounterStyleRegistry::resolveReferencesIfNeeded()
{
    // FIXME: does this need thread-safety guards?
    if (!hasUnresolvedReferences)
        return;

    for (auto& [name, counter] : m_authorCounterStyles) {
        if (counter->isFallbackUnresolved())
            resolveFallbackReference(*counter);
        if (counter->isExtendsSystem() && counter->isExtendsUnresolved())
            resolveExtendsReference(*counter);
    }
    hasUnresolvedReferences = false;
}

void CSSCounterStyleRegistry::resolveExtendsReference(CSSCounterStyle& counterStyle)
{
    HashSet<CSSCounterStyle*> countersInChain;
    resolveExtendsReference(counterStyle, countersInChain);
}

void CSSCounterStyleRegistry::resolveExtendsReference(CSSCounterStyle& counter, HashSet<CSSCounterStyle*>& countersInChain)
{
    ASSERT(counter.isExtendsSystem() && counter.isExtendsUnresolved());
    if (!(counter.isExtendsSystem() && counter.isExtendsUnresolved()))
        return;

    if (countersInChain.contains(&counter)) {
        // Chain of references forms a circle. Treat all as extending decimal (https://www.w3.org/TR/css-counter-styles-3/#extends-system).
        auto decimal = decimalCounter();
        for (const auto counterInChain : countersInChain) {
            ASSERT(counterInChain);
            if (!counterInChain)
                continue;
            counterInChain->extendAndResolve(*decimal);
        }
        // Recursion return for circular chain.
        return;
    }
    countersInChain.add(&counter);

    auto extendedCounter = counterStyle(counter.extendsName());
    ASSERT(extendedCounter);
    if (!extendedCounter)
        return;

    if (extendedCounter->isExtendsSystem() && extendedCounter->isExtendsUnresolved())
        resolveExtendsReference(*extendedCounter, countersInChain);

    // Recursion return for non-circular chain. Calling resolveExtendsReference() for the extendedCounter might have already resolved this counter style if a circle was formed. If it is still unresolved, it should get resolved here.
    if (counter.isExtendsUnresolved())
        counter.extendAndResolve(*extendedCounter);
}

void CSSCounterStyleRegistry::resolveFallbackReference(CSSCounterStyle& counter)
{
    counter.setFallbackReference(counterStyle(counter.fallbackName()));
}

void CSSCounterStyleRegistry::addCounterStyle(const CSSCounterStyleDescriptors& descriptors)
{
    hasUnresolvedReferences = true;
    m_authorCounterStyles.set(descriptors.m_name, CSSCounterStyle::create(descriptors, false));
}

RefPtr<CSSCounterStyle> CSSCounterStyleRegistry::decimalCounter() const
{
    auto& userAgentCounters = userAgentCounterStyles();
    auto iterator = userAgentCounters.find("decimal"_s);
    if (iterator != userAgentCounters.end())
        return iterator->value.get();
    // user agent counter style should always be populated with a counter named decimal
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CSSCounterStyle> CSSCounterStyleRegistry::counterStyle(const AtomString& name)
{
    if (name.isEmpty())
        return decimalCounter();

    auto getCounter = [&](const CounterStyleMap& map, const AtomString& counterName) {
        auto counterIterator = map.find(counterName);
        return counterIterator != map.end() ? counterIterator->value.get() : nullptr;
    };

    auto authorCounter = getCounter(m_authorCounterStyles, name);
    if (authorCounter)
        return authorCounter;
    auto userAgentCounter = getCounter(userAgentCounterStyles(), name);
    if (userAgentCounter)
        return userAgentCounter;

    return decimalCounter();
}

RefPtr<CSSCounterStyle> CSSCounterStyleRegistry::resolvedCounterStyle(const AtomString& name)
{
    resolveReferencesIfNeeded();
    return counterStyle(name);
}

CounterStyleMap& CSSCounterStyleRegistry::userAgentCounterStyles()
{
    // FIXME: counter-style should get pre-defined counters from UA stylesheet rdar://103021161.
    auto initialStyles = []() {
        CounterStyleMap map;
        map.add("decimal"_s, CSSCounterStyle::createCounterStyleDecimal());
        return map;
    };

    static NeverDestroyed<CounterStyleMap> counters = initialStyles();
    return counters;
}

bool CSSCounterStyleRegistry::operator==(const CSSCounterStyleRegistry& other) const
{
    return m_authorCounterStyles == other.m_authorCounterStyles;
}

}
