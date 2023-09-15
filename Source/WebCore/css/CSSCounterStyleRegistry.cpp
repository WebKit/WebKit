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
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"
#include "ListStyleType.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSCounterStyleRegistry);


void CSSCounterStyleRegistry::resolveUserAgentReferences()
{
    for (auto& [name, counter] : userAgentCounterStyles()) {
        // decimal counter has no fallback or extended references because it is the last resource for both cases.
        if (counter->name() == "decimal"_s)
            continue;
        if (counter->isFallbackUnresolved())
            resolveFallbackReference(*counter);
        if (counter->isExtendsSystem() && counter->isExtendsUnresolved())
            resolveExtendsReference(*counter);
    }
}
void CSSCounterStyleRegistry::resolveReferencesIfNeeded()
{
    if (!m_hasUnresolvedReferences)
        return;

    for (auto& [name, counter] : m_authorCounterStyles) {
        if (counter->isFallbackUnresolved())
            resolveFallbackReference(*counter, &m_authorCounterStyles);
        if (counter->isExtendsSystem() && counter->isExtendsUnresolved())
            resolveExtendsReference(*counter, &m_authorCounterStyles);
    }
    m_hasUnresolvedReferences = false;
}

void CSSCounterStyleRegistry::resolveExtendsReference(CSSCounterStyle& counterStyle, CounterStyleMap* map)
{
    HashSet<CSSCounterStyle*> countersInChain;
    resolveExtendsReference(counterStyle, countersInChain, map);
}

void CSSCounterStyleRegistry::resolveExtendsReference(CSSCounterStyle& counter, HashSet<CSSCounterStyle*>& countersInChain, CounterStyleMap* map)
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

    auto extendedCounter = counterStyle(counter.extendsName(), map);
    ASSERT(extendedCounter);
    if (!extendedCounter)
        return;

    if (extendedCounter->isExtendsSystem() && extendedCounter->isExtendsUnresolved())
        resolveExtendsReference(*extendedCounter, countersInChain, map);

    // Recursion return for non-circular chain. Calling resolveExtendsReference() for the extendedCounter might have already resolved this counter style if a circle was formed. If it is still unresolved, it should get resolved here.
    if (counter.isExtendsUnresolved())
        counter.extendAndResolve(*extendedCounter);
}

void CSSCounterStyleRegistry::resolveFallbackReference(CSSCounterStyle& counter, CounterStyleMap* map)
{
    counter.setFallbackReference(counterStyle(counter.fallbackName(), map));
}

void CSSCounterStyleRegistry::addCounterStyle(const CSSCounterStyleDescriptors& descriptors)
{
    m_hasUnresolvedReferences = true;
    m_authorCounterStyles.set(descriptors.m_name, CSSCounterStyle::create(descriptors, false));
}

void CSSCounterStyleRegistry::addUserAgentCounterStyle(const CSSCounterStyleDescriptors& descriptors)
{
    userAgentCounterStyles().set(descriptors.m_name, CSSCounterStyle::create(descriptors, true));
}

RefPtr<CSSCounterStyle> CSSCounterStyleRegistry::decimalCounter()
{
    auto& userAgentCounters = userAgentCounterStyles();
    auto iterator = userAgentCounters.find("decimal"_s);
    if (iterator != userAgentCounters.end())
        return iterator->value.get();
    // user agent counter style should always be populated with a counter named decimal if counter-style-at-rule is enabled
    return nullptr;
}

// A valid map means that the search begins at the author counter style map, otherwise we skip the search to the UA counter styles.
RefPtr<CSSCounterStyle> CSSCounterStyleRegistry::counterStyle(const AtomString& name, CounterStyleMap* map)
{
    if (name.isEmpty())
        return decimalCounter();

    auto getCounter = [&](const AtomString& counterName, const CounterStyleMap& map) {
        auto counterIterator = map.find(counterName);
        return counterIterator != map.end() ? counterIterator->value.get() : nullptr;
    };

    // If there is a map, the search starts from the given map.
    if (map) {
        auto counter = getCounter(name, *map);
        if (counter)
            return counter;
    }
    // If there was no map (called for user-agent references resolution), or the counter was not found in the given map, we search at the user-agent map.
    auto userAgentCounter = getCounter(name, userAgentCounterStyles());
    return userAgentCounter ? userAgentCounter : decimalCounter();
}

RefPtr<CSSCounterStyle> CSSCounterStyleRegistry::resolvedCounterStyle(const ListStyleType& listStyleType)
{
    if (listStyleType.type != ListStyleType::Type::CounterStyle)
        return nullptr;
    resolveReferencesIfNeeded();
    return counterStyle(listStyleType.identifier, &m_authorCounterStyles);
}

CounterStyleMap& CSSCounterStyleRegistry::userAgentCounterStyles()
{
    static NeverDestroyed<CounterStyleMap> counters;
    return counters;
}

bool CSSCounterStyleRegistry::operator==(const CSSCounterStyleRegistry& other) const
{
    // Intentionally doesn't check m_hasUnresolvedReferences.
    return m_authorCounterStyles == other.m_authorCounterStyles;
}

void CSSCounterStyleRegistry::clearAuthorCounterStyles()
{
    if (m_authorCounterStyles.isEmpty())
        return;
    m_authorCounterStyles.clear();
    invalidate();
}

void CSSCounterStyleRegistry::invalidate()
{
    m_hasUnresolvedReferences = true;
}

}
