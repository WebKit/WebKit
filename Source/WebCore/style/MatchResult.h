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
#include "StyleProperties.h"
#include "StyleScopeOrdinal.h"
#include <wtf/Hasher.h>

namespace WebCore::Style {

enum class FromStyleAttribute : bool { No, Yes };

struct MatchedProperties {
    RefPtr<const StyleProperties> properties;
    uint8_t linkMatchType { SelectorChecker::MatchAll };
    PropertyAllowlist allowlistType { PropertyAllowlist::None };
    ScopeOrdinal styleScopeOrdinal { ScopeOrdinal::Element };
    FromStyleAttribute fromStyleAttribute { FromStyleAttribute::No };
    CascadeLayerPriority cascadeLayerPriority { RuleSet::cascadeLayerPriorityForUnlayered };
};

struct MatchResult {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    bool isCacheable { true };
    Vector<MatchedProperties> userAgentDeclarations;
    Vector<MatchedProperties> userDeclarations;
    Vector<MatchedProperties> authorDeclarations;

    bool isEmpty() const { return userAgentDeclarations.isEmpty() && userDeclarations.isEmpty() && authorDeclarations.isEmpty(); }
};

inline bool operator==(const MatchResult& a, const MatchResult& b)
{
    return a.isCacheable == b.isCacheable
        && a.userAgentDeclarations == b.userAgentDeclarations
        && a.userDeclarations == b.userDeclarations
        && a.authorDeclarations == b.authorDeclarations;
}

inline bool operator!=(const MatchResult& a, const MatchResult& b)
{
    return !(a == b);
}

inline bool operator==(const MatchedProperties& a, const MatchedProperties& b)
{
    return a.properties == b.properties
        && a.linkMatchType == b.linkMatchType
        && a.allowlistType == b.allowlistType
        && a.styleScopeOrdinal == b.styleScopeOrdinal
        && a.fromStyleAttribute == b.fromStyleAttribute
        && a.cascadeLayerPriority == b.cascadeLayerPriority;
}

inline bool operator!=(const MatchedProperties& a, const MatchedProperties& b)
{
    return !(a == b);
}

inline void add(Hasher& hasher, const MatchedProperties& matchedProperties)
{
    add(hasher,
        matchedProperties.properties.get(),
        matchedProperties.linkMatchType,
        matchedProperties.allowlistType,
        matchedProperties.styleScopeOrdinal,
        matchedProperties.fromStyleAttribute,
        matchedProperties.cascadeLayerPriority
    );
}

inline void add(Hasher& hasher, const MatchResult& matchResult)
{
    add(hasher, matchResult.userAgentDeclarations, matchResult.userDeclarations, matchResult.authorDeclarations);
}

}
