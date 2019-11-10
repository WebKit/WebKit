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

#include "CascadeLevel.h"
#include "ElementRuleCollector.h"
#include "StyleBuilderState.h"
#include <bitset>

namespace WebCore {

class StyleResolver;

namespace Style {

class PropertyCascade {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum IncludedProperties { All, InheritedOnly };

    struct Direction {
        TextDirection textDirection;
        WritingMode writingMode;
    };

    PropertyCascade(const MatchResult&, OptionSet<CascadeLevel>, IncludedProperties, Direction);
    PropertyCascade(const PropertyCascade&, OptionSet<CascadeLevel>);

    ~PropertyCascade();

    struct Property {
        CSSPropertyID id;
        CascadeLevel level;
        ScopeOrdinal styleScopeOrdinal;
        CSSValue* cssValue[3]; // Values for link match states MatchDefault, MatchLink and MatchVisited
    };

    bool hasProperty(CSSPropertyID) const;
    const Property& property(CSSPropertyID) const;

    bool hasCustomProperty(const String&) const;
    Property customProperty(const String&) const;

    const Vector<Property, 8>& deferredProperties() const { return m_deferredProperties; }
    const HashMap<AtomString, Property>& customProperties() const { return m_customProperties; }

    Direction direction() const { return m_direction; }

    const PropertyCascade* propertyCascadeForRollback(CascadeLevel) const;

private:
    void buildCascade(OptionSet<CascadeLevel>);
    bool addNormalMatches(CascadeLevel);
    void addImportantMatches(CascadeLevel);
    bool addMatch(const MatchedProperties&, CascadeLevel, bool important);

    void set(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);
    void setDeferred(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);
    static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);

    Direction resolveDirectionAndWritingMode(Direction inheritedDirection) const;

    const MatchResult& m_matchResult;
    const IncludedProperties m_includedProperties;
    const Direction m_direction;

    Property m_properties[numCSSProperties + 2];
    std::bitset<numCSSProperties + 2> m_propertyIsPresent;

    Vector<Property, 8> m_deferredProperties;
    HashMap<AtomString, Property> m_customProperties;

    mutable std::unique_ptr<const PropertyCascade> m_authorRollbackCascade;
    mutable std::unique_ptr<const PropertyCascade> m_userRollbackCascade;
};

inline bool PropertyCascade::hasProperty(CSSPropertyID id) const
{
    ASSERT(id < m_propertyIsPresent.size());
    return m_propertyIsPresent[id];
}

inline const PropertyCascade::Property& PropertyCascade::property(CSSPropertyID id) const
{
    return m_properties[id];
}

inline bool PropertyCascade::hasCustomProperty(const String& name) const
{
    return m_customProperties.contains(name);
}

inline PropertyCascade::Property PropertyCascade::customProperty(const String& name) const
{
    return m_customProperties.get(name);
}

}
}
