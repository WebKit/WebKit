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
    PropertyCascade(StyleResolver&, const MatchResult&, OptionSet<CascadeLevel>, IncludedProperties = IncludedProperties::All);
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

    void applyProperties(int firstProperty, int lastProperty);
    void applyDeferredProperties();
    void applyCustomProperties();
    void applyCustomProperty(const String& name);
    void applyProperty(CSSPropertyID, CSSValue&, SelectorChecker::LinkMatchMask = SelectorChecker::MatchDefault);

    BuilderState& builderState() { return *m_builderState; }

private:
    void buildCascade(OptionSet<CascadeLevel>);
    bool addNormalMatches(CascadeLevel);
    void addImportantMatches(CascadeLevel);
    bool addMatch(const MatchedProperties&, CascadeLevel, bool important);

    void set(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);
    void setDeferred(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);
    static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);

    const PropertyCascade* propertyCascadeForRollback(CascadeLevel);

    enum CustomPropertyCycleTracking { Enabled = 0, Disabled };
    template<CustomPropertyCycleTracking trackCycles>
    void applyPropertiesImpl(int firstProperty, int lastProperty);
    void applyProperty(const Property&);

    Ref<CSSValue> resolveValue(CSSPropertyID, CSSValue&);
    RefPtr<CSSValue> resolvedVariableValue(CSSPropertyID, const CSSValue&);

    void resolveDirectionAndWritingMode();

    const MatchResult& m_matchResult;
    const IncludedProperties m_includedProperties;

    TextDirection m_direction;
    WritingMode m_writingMode;

    Property m_properties[numCSSProperties + 2];
    std::bitset<numCSSProperties + 2> m_propertyIsPresent;

    Vector<Property, 8> m_deferredProperties;
    HashMap<AtomString, Property> m_customProperties;

    std::unique_ptr<const PropertyCascade> m_authorRollbackCascade;
    std::unique_ptr<const PropertyCascade> m_userRollbackCascade;

    Optional<BuilderState> m_builderState;
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
