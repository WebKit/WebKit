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
#include "MatchResult.h"
#include <bitset>

namespace WebCore {

class StyleResolver;

namespace Style {

class PropertyCascade {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum IncludedProperties { All, InheritedOnly };

    PropertyCascade(const MatchResult&, CascadeLevel, IncludedProperties);
    PropertyCascade(const PropertyCascade&, CascadeLevel, std::optional<CascadeLayerPriority> maximumCascadeLayerPriorityForRollback = { });

    ~PropertyCascade();

    struct Property {
        CSSPropertyID id;
        CascadeLevel cascadeLevel;
        ScopeOrdinal styleScopeOrdinal;
        CascadeLayerPriority cascadeLayerPriority;
        FromStyleAttribute fromStyleAttribute;
        CSSValue* cssValue[3]; // Values for link match states MatchDefault, MatchLink and MatchVisited
    };

    bool hasProperty(CSSPropertyID) const;
    const Property& property(CSSPropertyID) const;

    bool hasDeferredProperty(CSSPropertyID) const;
    bool areDeferredInOrder(CSSPropertyID, CSSPropertyID) const;
    const Property& deferredProperty(CSSPropertyID) const;

    bool hasCustomProperty(const String&) const;
    Property customProperty(const String&) const;

    const Vector<Property, 8>& deferredProperties() const { return m_deferredProperties; }
    const HashMap<AtomString, Property>& customProperties() const { return m_customProperties; }

private:
    void buildCascade();
    bool addNormalMatches(CascadeLevel);
    void addImportantMatches(CascadeLevel);
    bool addMatch(const MatchedProperties&, CascadeLevel, bool important);

    void set(CSSPropertyID, CSSValue&, const MatchedProperties&, CascadeLevel);
    void setDeferred(CSSPropertyID, CSSValue&, const MatchedProperties&, CascadeLevel);
    static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, const MatchedProperties&, CascadeLevel);

    const MatchResult& m_matchResult;
    const IncludedProperties m_includedProperties;
    const CascadeLevel m_maximumCascadeLevel;
    const std::optional<CascadeLayerPriority> m_maximumCascadeLayerPriorityForRollback;

    Property m_properties[numCSSProperties + 2];
    std::bitset<numCSSProperties + 2> m_propertyIsPresent;

    Vector<Property, 8> m_deferredProperties;
    HashMap<CSSPropertyID, unsigned> m_deferredPropertiesIndices;

    HashMap<AtomString, Property> m_customProperties;
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

inline bool PropertyCascade::hasDeferredProperty(CSSPropertyID id) const
{
    return m_deferredPropertiesIndices.contains(id);
}

inline bool PropertyCascade::areDeferredInOrder(CSSPropertyID id1, CSSPropertyID id2) const
{
    ASSERT(hasDeferredProperty(id1));
    ASSERT(hasDeferredProperty(id2));
    return m_deferredPropertiesIndices.get(id1) < m_deferredPropertiesIndices.get(id2);
}

inline const PropertyCascade::Property& PropertyCascade::deferredProperty(CSSPropertyID id) const
{
    ASSERT(hasDeferredProperty(id));
    unsigned index = m_deferredPropertiesIndices.get(id);
    ASSERT(index < m_deferredProperties.size());
    return m_deferredProperties[index];
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
