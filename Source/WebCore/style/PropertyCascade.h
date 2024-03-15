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
#include "WebAnimationTypes.h"
#include <bitset>

namespace WebCore {

class StyleResolver;

namespace Style {

class PropertyCascade {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class PropertyType : uint8_t {
        NonInherited = 1 << 0,
        Inherited = 1 << 1,
        ExplicitlyInherited = 1 << 2,
        AfterAnimation = 1 << 3,
        AfterTransition = 1 << 4,
        StartingStyle = 1 << 5,
    };
    static constexpr OptionSet<PropertyType> normalProperties() { return { PropertyType::NonInherited,  PropertyType::Inherited }; }
    static constexpr OptionSet<PropertyType> startingStyleProperties() { return normalProperties() | PropertyType::StartingStyle; }

    PropertyCascade(const MatchResult&, CascadeLevel, OptionSet<PropertyType> includedProperties, const HashSet<AnimatableCSSProperty>* = nullptr);
    PropertyCascade(const PropertyCascade&, CascadeLevel, std::optional<ScopeOrdinal> rollbackScope = { }, std::optional<CascadeLayerPriority> maximumCascadeLayerPriorityForRollback = { });

    ~PropertyCascade();

    struct Property {
        CSSPropertyID id;
        CascadeLevel cascadeLevel;
        ScopeOrdinal styleScopeOrdinal;
        CascadeLayerPriority cascadeLayerPriority;
        FromStyleAttribute fromStyleAttribute;
        std::array<CSSValue*, 3> cssValue; // Values for link match states MatchDefault, MatchLink and MatchVisited
    };

    bool isEmpty() const { return m_propertyIsPresent.none() && !m_seenDeferredPropertyCount; }

    bool hasNormalProperty(CSSPropertyID) const;
    const Property& normalProperty(CSSPropertyID) const;

    bool hasDeferredProperty(CSSPropertyID) const;
    const Property& deferredProperty(CSSPropertyID) const;
    const Property* lastDeferredPropertyResolvingRelated(CSSPropertyID, TextDirection, WritingMode) const;

    bool hasCustomProperty(const AtomString&) const;
    const Property& customProperty(const AtomString&) const;

    std::span<const CSSPropertyID> deferredPropertyIDs() const;
    const HashMap<AtomString, Property>& customProperties() const { return m_customProperties; }

    const HashSet<AnimatableCSSProperty> overriddenAnimatedProperties() const;

private:
    void buildCascade();
    bool addNormalMatches(CascadeLevel);
    void addImportantMatches(CascadeLevel);
    bool addMatch(const MatchedProperties&, CascadeLevel, bool important);
    bool shouldApplyAfterAnimation(const StyleProperties::PropertyReference&);

    void set(CSSPropertyID, CSSValue&, const MatchedProperties&, CascadeLevel);
    void setDeferred(CSSPropertyID, CSSValue&, const MatchedProperties&, CascadeLevel);
    static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, const MatchedProperties&, CascadeLevel);

    bool hasProperty(CSSPropertyID, const CSSValue&);

    unsigned deferredPropertyIndex(CSSPropertyID) const;
    void setDeferredPropertyIndex(CSSPropertyID, unsigned);
    void sortDeferredPropertyIDs();

    const MatchResult& m_matchResult;
    const OptionSet<PropertyType> m_includedProperties;
    const CascadeLevel m_maximumCascadeLevel;
    const std::optional<ScopeOrdinal> m_rollbackScope;
    const std::optional<CascadeLayerPriority> m_maximumCascadeLayerPriorityForRollback;

    struct AnimationLayer {
        AnimationLayer(const HashSet<AnimatableCSSProperty>&);

        const HashSet<AnimatableCSSProperty>& properties;
        HashSet<AnimatableCSSProperty> overriddenProperties;
        bool hasCustomProperties { false };
        bool hasFontSize { false };
        bool hasLineHeight { false };
    };
    std::optional<AnimationLayer> m_animationLayer;

    // The CSSPropertyID enum is sorted like this:
    // 1. CSSPropertyInvalid and CSSPropertyCustom.
    // 2. Normal longhand properties (high priority ones followed by low priority ones).
    // 3. Deferred longhand properties.
    // 4. Shorthand properties.
    //
    // 'm_properties' is used for both normal and deferred longhands, so it has size 'lastDeferredProperty + 1'.
    // It could actually be 2 units smaller, but then we would have to subtract 'firstCSSProperty', which may not be worth it.
    // 'm_propertyIsPresent' is not used for deferred properties, so we only need to cover up to the last low priority one.
    std::array<Property, lastDeferredProperty + 1> m_properties;
    std::bitset<lastLowPriorityProperty + 1> m_propertyIsPresent;

    static constexpr unsigned deferredPropertyCount = lastDeferredProperty - firstDeferredProperty + 1;
    std::array<unsigned, deferredPropertyCount> m_deferredPropertyIndices { };
    unsigned m_lastIndexForDeferred { 0 };
    std::array<CSSPropertyID, deferredPropertyCount> m_deferredPropertyIDs { };
    unsigned m_seenDeferredPropertyCount { 0 };
    CSSPropertyID m_lowestSeenDeferredProperty { lastDeferredProperty };
    CSSPropertyID m_highestSeenDeferredProperty { firstDeferredProperty };

    HashMap<AtomString, Property> m_customProperties;
};

inline bool PropertyCascade::hasNormalProperty(CSSPropertyID id) const
{
    ASSERT(id < firstDeferredProperty);
    return m_propertyIsPresent[id];
}

inline const PropertyCascade::Property& PropertyCascade::normalProperty(CSSPropertyID id) const
{
    ASSERT(hasNormalProperty(id));
    return m_properties[id];
}

inline unsigned PropertyCascade::deferredPropertyIndex(CSSPropertyID id) const
{
    ASSERT(id >= firstDeferredProperty);
    ASSERT(id <= lastDeferredProperty);
    return m_deferredPropertyIndices[id - firstDeferredProperty];
}

inline void PropertyCascade::setDeferredPropertyIndex(CSSPropertyID id, unsigned index)
{
    ASSERT(id >= firstDeferredProperty);
    ASSERT(id <= lastDeferredProperty);
    m_deferredPropertyIndices[id - firstDeferredProperty] = index;
}

inline bool PropertyCascade::hasDeferredProperty(CSSPropertyID id) const
{
    return deferredPropertyIndex(id);
}

inline const PropertyCascade::Property& PropertyCascade::deferredProperty(CSSPropertyID id) const
{
    ASSERT(hasDeferredProperty(id));
    return m_properties[id];
}

inline std::span<const CSSPropertyID> PropertyCascade::deferredPropertyIDs() const
{
    return { m_deferredPropertyIDs.data(), m_seenDeferredPropertyCount };
}

inline bool PropertyCascade::hasCustomProperty(const AtomString& name) const
{
    return m_customProperties.contains(name);
}

inline const PropertyCascade::Property& PropertyCascade::customProperty(const AtomString& name) const
{
    ASSERT(hasCustomProperty(name));
    return m_customProperties.find(name)->value;
}

}
}
