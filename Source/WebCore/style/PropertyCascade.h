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

#include "ElementRuleCollector.h"
#include <bitset>
#include <wtf/Bitmap.h>

namespace WebCore {

class StyleResolver;
struct ApplyCascadedPropertyState;

namespace Style {

enum class CascadeLevel : uint8_t {
    UserAgent,
    User,
    Author
};

class PropertyCascade {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PropertyCascade(TextDirection, WritingMode);
    ~PropertyCascade();

    struct Property {
        void apply(StyleResolver&, ApplyCascadedPropertyState&);

        CSSPropertyID id;
        CascadeLevel level;
        ScopeOrdinal styleScopeOrdinal;
        CSSValue* cssValue[3];
    };

    bool hasProperty(CSSPropertyID) const;
    Property& property(CSSPropertyID);

    void addNormalMatches(const MatchResult&, CascadeLevel, bool inheritedOnly = false);
    void addImportantMatches(const MatchResult&, CascadeLevel, bool inheritedOnly = false);

    void applyDeferredProperties(StyleResolver&, ApplyCascadedPropertyState&);

    HashMap<AtomString, Property>& customProperties() { return m_customProperties; }
    bool hasCustomProperty(const String&) const;
    Property customProperty(const String&) const;

private:
    void addMatch(const MatchedProperties&, CascadeLevel, bool isImportant, bool inheritedOnly);
    void set(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);
    void setDeferred(CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);
    static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, unsigned linkMatchType, CascadeLevel, ScopeOrdinal);

    Property m_properties[numCSSProperties + 2];
    std::bitset<numCSSProperties + 2> m_propertyIsPresent;

    Vector<Property, 8> m_deferredProperties;
    HashMap<AtomString, Property> m_customProperties;

    TextDirection m_direction;
    WritingMode m_writingMode;
};

inline bool PropertyCascade::hasProperty(CSSPropertyID id) const
{
    ASSERT(id < m_propertyIsPresent.size());
    return m_propertyIsPresent[id];
}

inline PropertyCascade::Property& PropertyCascade::property(CSSPropertyID id)
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
