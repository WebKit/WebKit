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

#include "config.h"
#include "PropertyCascade.h"

#include "StyleResolver.h"

namespace WebCore {
namespace Style {

static inline bool shouldApplyPropertyInParseOrder(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitTextDecoration:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextDecorationSkip:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyTextUnderlineOffset:
    case CSSPropertyTextDecorationThickness:
    case CSSPropertyTextDecoration:
        return true;
    default:
        return false;
    }
}

// https://www.w3.org/TR/css-pseudo-4/#marker-pseudo (Editor's Draft, 25 July 2017)
static inline bool isValidMarkerStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyColor:
    case CSSPropertyFontFamily:
    case CSSPropertyFontFeatureSettings:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontSynthesis:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontWeight:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
    case CSSPropertyFontVariationSettings:
#endif
        return true;
    default:
        break;
    }
    return false;
}

#if ENABLE(VIDEO_TRACK)
static inline bool isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
    case CSSPropertyPaintOrder:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeColor:
    case CSSPropertyStrokeWidth:
        return true;
    default:
        break;
    }
    return false;
}
#endif

PropertyCascade::PropertyCascade(StyleResolver& styleResolver, const MatchResult& matchResult, TextDirection direction, WritingMode writingMode)
    : m_styleResolver(styleResolver)
    , m_matchResult(matchResult)
    , m_direction(direction)
    , m_writingMode(writingMode)
{
}

PropertyCascade::~PropertyCascade() = default;

void PropertyCascade::setPropertyInternal(Property& property, CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel, ScopeOrdinal styleScopeOrdinal)
{
    ASSERT(linkMatchType <= SelectorChecker::MatchAll);
    property.id = id;
    property.level = cascadeLevel;
    property.styleScopeOrdinal = styleScopeOrdinal;
    if (linkMatchType == SelectorChecker::MatchAll) {
        property.cssValue[0] = &cssValue;
        property.cssValue[SelectorChecker::MatchLink] = &cssValue;
        property.cssValue[SelectorChecker::MatchVisited] = &cssValue;
    } else
        property.cssValue[linkMatchType] = &cssValue;
}

void PropertyCascade::set(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel, ScopeOrdinal styleScopeOrdinal)
{
    if (CSSProperty::isDirectionAwareProperty(id))
        id = CSSProperty::resolveDirectionAwareProperty(id, m_direction, m_writingMode);

    ASSERT(!shouldApplyPropertyInParseOrder(id));

    auto& property = m_properties[id];
    ASSERT(id < m_propertyIsPresent.size());
    if (id == CSSPropertyCustom) {
        m_propertyIsPresent.set(id);
        const auto& customValue = downcast<CSSCustomPropertyValue>(cssValue);
        bool hasValue = m_customProperties.contains(customValue.name());
        if (!hasValue) {
            Property property;
            property.id = id;
            memset(property.cssValue, 0, sizeof(property.cssValue));
            setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
            m_customProperties.set(customValue.name(), property);
        } else {
            Property property = customProperty(customValue.name());
            setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
            m_customProperties.set(customValue.name(), property);
        }
        return;
    }

    if (!m_propertyIsPresent[id])
        memset(property.cssValue, 0, sizeof(property.cssValue));
    m_propertyIsPresent.set(id);
    setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
}

void PropertyCascade::setDeferred(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType, CascadeLevel cascadeLevel, ScopeOrdinal styleScopeOrdinal)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(id));
    ASSERT(shouldApplyPropertyInParseOrder(id));

    Property property;
    memset(property.cssValue, 0, sizeof(property.cssValue));
    setPropertyInternal(property, id, cssValue, linkMatchType, cascadeLevel, styleScopeOrdinal);
    m_deferredProperties.append(property);
}


void PropertyCascade::addMatch(const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel, bool isImportant, bool inheritedOnly)
{
    auto& styleProperties = *matchedProperties.properties;
    auto propertyWhitelistType = static_cast<PropertyWhitelistType>(matchedProperties.whitelistType);

    for (unsigned i = 0, count = styleProperties.propertyCount(); i < count; ++i) {
        auto current = styleProperties.propertyAt(i);
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // Inherited only mode is used after matched properties cache hit.
            // A match with a value that is explicitly inherited should never have been cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }
        CSSPropertyID propertyID = current.id();

#if ENABLE(VIDEO_TRACK)
        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(propertyID))
            continue;
#endif
        if (propertyWhitelistType == PropertyWhitelistMarker && !isValidMarkerStyleProperty(propertyID))
            continue;

        if (shouldApplyPropertyInParseOrder(propertyID))
            setDeferred(propertyID, *current.value(), matchedProperties.linkMatchType, cascadeLevel, matchedProperties.styleScopeOrdinal);
        else
            set(propertyID, *current.value(), matchedProperties.linkMatchType, cascadeLevel, matchedProperties.styleScopeOrdinal);
    }
}

static auto& declarationsForCascadeLevel(const MatchResult& matchResult, CascadeLevel cascadeLevel)
{
    switch (cascadeLevel) {
    case CascadeLevel::UserAgent: return matchResult.userAgentDeclarations;
    case CascadeLevel::User: return matchResult.userDeclarations;
    case CascadeLevel::Author: return matchResult.authorDeclarations;
    }
    ASSERT_NOT_REACHED();
    return matchResult.authorDeclarations;
}

void PropertyCascade::addNormalMatches(CascadeLevel cascadeLevel, bool inheritedOnly)
{
    for (auto& matchedDeclarations : declarationsForCascadeLevel(m_matchResult, cascadeLevel))
        addMatch(matchedDeclarations, cascadeLevel, false, inheritedOnly);
}

static bool hasImportantProperties(const StyleProperties& properties)
{
    for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
        if (properties.propertyAt(i).isImportant())
            return true;
    }
    return false;
}

void PropertyCascade::addImportantMatches(CascadeLevel cascadeLevel, bool inheritedOnly)
{
    struct IndexAndOrdinal {
        unsigned index;
        ScopeOrdinal ordinal;
    };
    Vector<IndexAndOrdinal> importantMatches;
    bool hasMatchesFromOtherScopes = false;

    auto& matchedDeclarations = declarationsForCascadeLevel(m_matchResult, cascadeLevel);

    for (unsigned i = 0; i < matchedDeclarations.size(); ++i) {
        const MatchedProperties& matchedProperties = matchedDeclarations[i];

        if (!hasImportantProperties(*matchedProperties.properties))
            continue;

        importantMatches.append({ i, matchedProperties.styleScopeOrdinal });

        if (matchedProperties.styleScopeOrdinal != ScopeOrdinal::Element)
            hasMatchesFromOtherScopes = true;
    }

    if (importantMatches.isEmpty())
        return;

    if (hasMatchesFromOtherScopes) {
        // For !important properties a later shadow tree wins.
        // Match results are sorted in reverse tree context order so this is not needed for normal properties.
        std::stable_sort(importantMatches.begin(), importantMatches.end(), [] (const IndexAndOrdinal& a, const IndexAndOrdinal& b) {
            return a.ordinal < b.ordinal;
        });
    }

    for (auto& match : importantMatches)
        addMatch(matchedDeclarations[match.index], cascadeLevel, true, inheritedOnly);
}

void PropertyCascade::applyDeferredProperties()
{
    for (auto& property : m_deferredProperties)
        property.apply(*this);
}

void PropertyCascade::applyProperties(int firstProperty, int lastProperty)
{
    if (LIKELY(m_customProperties.isEmpty()))
        return applyPropertiesImpl<CustomPropertyCycleTracking::Disabled>(firstProperty, lastProperty);

    return applyPropertiesImpl<CustomPropertyCycleTracking::Enabled>(firstProperty, lastProperty);
}

template<PropertyCascade::CustomPropertyCycleTracking trackCycles>
inline void PropertyCascade::applyPropertiesImpl(int firstProperty, int lastProperty)
{
    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!hasProperty(propertyID))
            continue;
        ASSERT(propertyID != CSSPropertyCustom);
        auto& property = m_properties[propertyID];

        if (trackCycles == CustomPropertyCycleTracking::Enabled) {
            if (UNLIKELY(m_applyState.inProgressProperties.get(propertyID))) {
                // We are in a cycle (eg. setting font size using registered custom property value containing em).
                // So this value should be unset.
                m_applyState.appliedProperties.set(propertyID);
                // This property is in a cycle, and only the root of the call stack will have firstProperty != lastProperty.
                ASSERT(firstProperty == lastProperty);
                continue;
            }

            m_applyState.inProgressProperties.set(propertyID);
            property.apply(*this);
            m_applyState.appliedProperties.set(propertyID);
            m_applyState.inProgressProperties.set(propertyID, false);
            continue;
        }

        // If we don't have any custom properties, then there can't be any cycles.
        property.apply(*this);
    }
}

void PropertyCascade::applyCustomProperties()
{
    for (auto& name : m_customProperties.keys())
        applyCustomProperty(name);
}

void PropertyCascade::applyCustomProperty(const String& name)
{
    if (m_applyState.appliedCustomProperties.contains(name) || !m_customProperties.contains(name))
        return;

    auto property = customProperty(name);
    bool inCycle = m_applyState.inProgressPropertiesCustom.contains(name);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && m_styleResolver.state().style()->insideLink() == InsideLink::NotInside)
            continue;

        Ref<CSSCustomPropertyValue> valueToApply = CSSCustomPropertyValue::create(downcast<CSSCustomPropertyValue>(*property.cssValue[index]));

        if (inCycle) {
            m_applyState.appliedCustomProperties.add(name); // Make sure we do not try to apply this property again while resolving it.
            valueToApply = CSSCustomPropertyValue::createWithID(name, CSSValueInvalid);
        }

        m_applyState.inProgressPropertiesCustom.add(name);

        if (WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            RefPtr<CSSValue> parsedValue = m_styleResolver.resolvedVariableValue(CSSPropertyCustom, valueToApply.get(), *this);

            if (m_applyState.appliedCustomProperties.contains(name))
                return; // There was a cycle and the value was reset, so bail.

            if (!parsedValue)
                parsedValue = CSSCustomPropertyValue::createWithID(name, CSSValueUnset);

            valueToApply = downcast<CSSCustomPropertyValue>(*parsedValue);
        }

        if (m_applyState.inProgressPropertiesCustom.contains(name)) {
            if (index == SelectorChecker::MatchDefault) {
                m_styleResolver.state().setApplyPropertyToRegularStyle(true);
                m_styleResolver.state().setApplyPropertyToVisitedLinkStyle(false);
            }

            if (index == SelectorChecker::MatchLink) {
                m_styleResolver.state().setApplyPropertyToRegularStyle(true);
                m_styleResolver.state().setApplyPropertyToVisitedLinkStyle(false);
            }

            if (index == SelectorChecker::MatchVisited) {
                m_styleResolver.state().setApplyPropertyToRegularStyle(false);
                m_styleResolver.state().setApplyPropertyToVisitedLinkStyle(true);
            }
            m_styleResolver.applyProperty(CSSPropertyCustom, valueToApply.ptr(), *this, index);
        }
    }

    m_applyState.inProgressPropertiesCustom.remove(name);
    m_applyState.appliedCustomProperties.add(name);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && m_styleResolver.state().style()->insideLink() == InsideLink::NotInside)
            continue;

        Ref<CSSCustomPropertyValue> valueToApply = CSSCustomPropertyValue::create(downcast<CSSCustomPropertyValue>(*property.cssValue[index]));

        if (inCycle && WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            // Resolve this value so that we reset its dependencies.
            m_styleResolver.resolvedVariableValue(CSSPropertyCustom, valueToApply.get(), *this);
        }
    }
}

PropertyCascade* PropertyCascade::propertyCascadeForRollback(CascadeLevel cascadeLevel)
{
    switch (cascadeLevel) {
    case CascadeLevel::Author:
        if (!m_authorRollbackCascade) {
            m_authorRollbackCascade = makeUnique<PropertyCascade>(m_styleResolver, m_matchResult, m_direction, m_writingMode);

            // This special rollback cascade contains UA rules and user rules but no author rules.
            m_authorRollbackCascade->addNormalMatches(CascadeLevel::UserAgent, false);
            m_authorRollbackCascade->addNormalMatches(CascadeLevel::User, false);
            m_authorRollbackCascade->addImportantMatches(CascadeLevel::User, false);
            m_authorRollbackCascade->addImportantMatches(CascadeLevel::UserAgent, false);
        }
        return m_authorRollbackCascade.get();

    case CascadeLevel::User:
        if (!m_userRollbackCascade) {
            m_userRollbackCascade = makeUnique<PropertyCascade>(m_styleResolver, m_matchResult, m_direction, m_writingMode);

            // This special rollback cascade contains only UA rules.
            m_userRollbackCascade->addNormalMatches(CascadeLevel::UserAgent, false);
            m_userRollbackCascade->addImportantMatches(CascadeLevel::UserAgent, false);
        }
        return m_userRollbackCascade.get();

    case CascadeLevel::UserAgent:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void PropertyCascade::Property::apply(PropertyCascade& cascade)
{
    auto& resolver = cascade.styleResolver();
    StyleResolver::State& state = resolver.state();
    state.setCascadeLevel(level);
    state.setStyleScopeOrdinal(styleScopeOrdinal);

    if (cssValue[SelectorChecker::MatchDefault]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchDefault], cascade, SelectorChecker::MatchDefault);
    }

    if (state.style()->insideLink() == InsideLink::NotInside)
        return;

    if (cssValue[SelectorChecker::MatchLink]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchLink], cascade, SelectorChecker::MatchLink);
    }

    if (cssValue[SelectorChecker::MatchVisited]) {
        state.setApplyPropertyToRegularStyle(false);
        state.setApplyPropertyToVisitedLinkStyle(true);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchVisited], cascade, SelectorChecker::MatchVisited);
    }

    state.setApplyPropertyToRegularStyle(true);
    state.setApplyPropertyToVisitedLinkStyle(false);
}

}
}
