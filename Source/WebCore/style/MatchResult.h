/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PropertyAllowlist.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "StylePropertiesInlines.h"
#include "StyleScopeOrdinal.h"
#include <wtf/Hasher.h>

namespace WebCore::Style {

enum class FromStyleAttribute : bool { No, Yes };
enum class IsCacheable : uint8_t { No, Partially, Yes };

struct MatchedProperties {
    Ref<const StyleProperties> properties;
    uint8_t linkMatchType { SelectorChecker::MatchAll };
    PropertyAllowlist allowlistType { PropertyAllowlist::None };
    ScopeOrdinal styleScopeOrdinal { ScopeOrdinal::Element };
    FromStyleAttribute fromStyleAttribute { FromStyleAttribute::No };
    CascadeLayerPriority cascadeLayerPriority { RuleSet::cascadeLayerPriorityForUnlayered };
    IsStartingStyle isStartingStyle { IsStartingStyle::No };
    IsCacheable isCacheable { IsCacheable::Yes };
};

struct MatchResult {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    MatchResult(bool isForLink = false)
        : isForLink(isForLink)
    { }

    bool isForLink { false };
    bool isCompletelyNonCacheable { false };
    bool hasStartingStyle { false };
    Vector<MatchedProperties> userAgentDeclarations;
    Vector<MatchedProperties> userDeclarations;
    Vector<MatchedProperties> authorDeclarations;
    Vector<CSSPropertyID, 4> nonCacheablePropertyIds;

    bool isEmpty() const { return userAgentDeclarations.isEmpty() && userDeclarations.isEmpty() && authorDeclarations.isEmpty(); }

    friend bool operator==(const MatchResult&, const MatchResult&) = default;
    bool cacheablePropertiesEqual(const MatchResult&) const;
};

inline bool operator==(const MatchedProperties& a, const MatchedProperties& b)
{
    return a.properties.ptr() == b.properties.ptr()
        && a.linkMatchType == b.linkMatchType
        && a.allowlistType == b.allowlistType
        && a.styleScopeOrdinal == b.styleScopeOrdinal
        && a.fromStyleAttribute == b.fromStyleAttribute
        && a.cascadeLayerPriority == b.cascadeLayerPriority
        && a.isStartingStyle == b.isStartingStyle
        && a.isCacheable == b.isCacheable;
}

inline bool MatchResult::cacheablePropertiesEqual(const MatchResult& other) const
{
    if (isForLink != other.isForLink || hasStartingStyle != other.hasStartingStyle)
        return false;

    // Only author style can be non-cacheable.
    if (userAgentDeclarations != other.userAgentDeclarations)
        return false;
    if (userDeclarations != other.userDeclarations)
        return false;

    // Currently the cached style contains also the non-cacheable property values from when the entry was made
    // so we can only allow styles that override the same exact properties. Content usually animates or varies the same
    // small set of properties so this doesn't make a significant difference.
    auto nonCacheableEqual = std::ranges::equal(nonCacheablePropertyIds, other.nonCacheablePropertyIds, [](auto& idA, auto& idB) {
        // This would need to check the custom property names for equality.
        if (idA == CSSPropertyCustom || idB == CSSPropertyCustom)
            return false;
        return idA == idB;
    });
    if (!nonCacheableEqual)
        return false;

    return std::ranges::equal(authorDeclarations, other.authorDeclarations, [](auto& propertiesA, auto& propertiesB) {
        if (propertiesA.isCacheable == IsCacheable::Partially && propertiesB.isCacheable == IsCacheable::Partially)
            return true;
        return propertiesA == propertiesB;
    });
}

inline void add(Hasher& hasher, const MatchedProperties& matchedProperties)
{
    // Ignore non-cacheable properties when computing hash.
    if (matchedProperties.isCacheable == IsCacheable::Partially)
        return;
    ASSERT(matchedProperties.isCacheable == IsCacheable::Yes);

    add(hasher,
        matchedProperties.properties.ptr(),
        matchedProperties.linkMatchType,
        matchedProperties.allowlistType,
        matchedProperties.styleScopeOrdinal,
        matchedProperties.fromStyleAttribute,
        matchedProperties.cascadeLayerPriority,
        matchedProperties.isStartingStyle
    );
}

inline void add(Hasher& hasher, const MatchResult& matchResult)
{
    ASSERT(!matchResult.isCompletelyNonCacheable);
    add(hasher, matchResult.isForLink, matchResult.nonCacheablePropertyIds, matchResult.userAgentDeclarations, matchResult.userDeclarations, matchResult.authorDeclarations);
}

}
