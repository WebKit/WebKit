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

#include "CSSPaintImageValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePool.h"
#include "PaintWorkletGlobalScope.h"
#include "StyleBuilder.h"
#include "StylePropertyShorthand.h"
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

static inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyFill:
    case CSSPropertyStroke:
    case CSSPropertyStrokeColor:
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

PropertyCascade::PropertyCascade(StyleResolver& styleResolver, const MatchResult& matchResult, OptionSet<CascadeLevel> cascadeLevels, IncludedProperties includedProperties)
    : m_matchResult(matchResult)
    , m_includedProperties(includedProperties)
    , m_builderState(std::in_place, *this, styleResolver)
{
    // Directional properties (*-before/after) are aliases that depend on the TextDirection and WritingMode.
    // These must be resolved before we can begin building the property cascade.
    resolveDirectionAndWritingMode();

    buildCascade(cascadeLevels);
}

PropertyCascade::PropertyCascade(const PropertyCascade& parent, OptionSet<CascadeLevel> cascadeLevels)
    :  m_matchResult(parent.m_matchResult)
    , m_includedProperties(parent.m_includedProperties)
    , m_direction(parent.m_direction)
    , m_writingMode(parent.m_writingMode)
{
    buildCascade(cascadeLevels);
}

PropertyCascade::~PropertyCascade() = default;

void PropertyCascade::buildCascade(OptionSet<CascadeLevel> cascadeLevels)
{
    OptionSet<CascadeLevel> cascadeLevelsWithImportant;

    for (auto cascadeLevel : cascadeLevels) {
        bool hasImportant = addNormalMatches(cascadeLevel);
        if (hasImportant)
            cascadeLevelsWithImportant.add(cascadeLevel);
    }

    for (auto cascadeLevel : { CascadeLevel::Author, CascadeLevel::User, CascadeLevel::UserAgent }) {
        if (!cascadeLevelsWithImportant.contains(cascadeLevel))
            continue;
        addImportantMatches(cascadeLevel);
    }
}

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


bool PropertyCascade::addMatch(const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel, bool important)
{
    auto& styleProperties = *matchedProperties.properties;
    auto propertyWhitelistType = static_cast<PropertyWhitelistType>(matchedProperties.whitelistType);
    bool hasImportantProperties = false;

    for (unsigned i = 0, count = styleProperties.propertyCount(); i < count; ++i) {
        auto current = styleProperties.propertyAt(i);

        if (current.isImportant())
            hasImportantProperties = true;
        if (important != current.isImportant())
            continue;

        if (m_includedProperties == IncludedProperties::InheritedOnly && !current.isInherited()) {
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

    return hasImportantProperties;
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

bool PropertyCascade::addNormalMatches(CascadeLevel cascadeLevel)
{
    bool hasImportant = false;
    for (auto& matchedDeclarations : declarationsForCascadeLevel(m_matchResult, cascadeLevel))
        hasImportant |= addMatch(matchedDeclarations, cascadeLevel, false);

    return hasImportant;
}

static bool hasImportantProperties(const StyleProperties& properties)
{
    for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
        if (properties.propertyAt(i).isImportant())
            return true;
    }
    return false;
}

void PropertyCascade::addImportantMatches(CascadeLevel cascadeLevel)
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
        addMatch(matchedDeclarations[match.index], cascadeLevel, true);
}

void PropertyCascade::applyDeferredProperties()
{
    for (auto& property : m_deferredProperties)
        applyProperty(property);
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
            if (UNLIKELY(builderState().m_inProgressProperties.get(propertyID))) {
                // We are in a cycle (eg. setting font size using registered custom property value containing em).
                // So this value should be unset.
                builderState().m_appliedProperties.set(propertyID);
                // This property is in a cycle, and only the root of the call stack will have firstProperty != lastProperty.
                ASSERT(firstProperty == lastProperty);
                continue;
            }
            auto& builderState = *m_builderState;
            builderState.m_inProgressProperties.set(propertyID);
            applyProperty(property);
            builderState.m_appliedProperties.set(propertyID);
            builderState.m_inProgressProperties.set(propertyID, false);
            continue;
        }

        // If we don't have any custom properties, then there can't be any cycles.
        applyProperty(property);
    }
}

void PropertyCascade::applyCustomProperties()
{
    for (auto& name : m_customProperties.keys())
        applyCustomProperty(name);
}

void PropertyCascade::applyCustomProperty(const String& name)
{
    auto& builderState = *m_builderState;
    if (builderState.m_appliedCustomProperties.contains(name) || !m_customProperties.contains(name))
        return;

    auto property = customProperty(name);
    bool inCycle = builderState.m_inProgressPropertiesCustom.contains(name);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && builderState.style().insideLink() == InsideLink::NotInside)
            continue;

        Ref<CSSCustomPropertyValue> valueToApply = CSSCustomPropertyValue::create(downcast<CSSCustomPropertyValue>(*property.cssValue[index]));

        if (inCycle) {
            builderState.m_appliedCustomProperties.add(name); // Make sure we do not try to apply this property again while resolving it.
            valueToApply = CSSCustomPropertyValue::createWithID(name, CSSValueInvalid);
        }

        builderState.m_inProgressPropertiesCustom.add(name);

        if (WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            RefPtr<CSSValue> parsedValue = resolvedVariableValue(CSSPropertyCustom, valueToApply.get());

            if (builderState.m_appliedCustomProperties.contains(name))
                return; // There was a cycle and the value was reset, so bail.

            if (!parsedValue)
                parsedValue = CSSCustomPropertyValue::createWithID(name, CSSValueUnset);

            valueToApply = downcast<CSSCustomPropertyValue>(*parsedValue);
        }

        if (builderState.m_inProgressPropertiesCustom.contains(name)) {
            builderState.m_linkMatch = index;
            applyProperty(CSSPropertyCustom, valueToApply.get(), index);
        }
    }

    builderState.m_linkMatch = SelectorChecker::MatchDefault;
    builderState.m_inProgressPropertiesCustom.remove(name);
    builderState.m_appliedCustomProperties.add(name);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && builderState.style().insideLink() == InsideLink::NotInside)
            continue;

        Ref<CSSCustomPropertyValue> valueToApply = CSSCustomPropertyValue::create(downcast<CSSCustomPropertyValue>(*property.cssValue[index]));

        if (inCycle && WTF::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            // Resolve this value so that we reset its dependencies.
            resolvedVariableValue(CSSPropertyCustom, valueToApply.get());
        }
    }
}

const PropertyCascade* PropertyCascade::propertyCascadeForRollback(CascadeLevel cascadeLevel)
{
    switch (cascadeLevel) {
    case CascadeLevel::Author:
        if (!m_authorRollbackCascade) {
            auto cascadeLevels = OptionSet<CascadeLevel> { CascadeLevel::UserAgent, CascadeLevel::User };
            m_authorRollbackCascade = makeUnique<const PropertyCascade>(*this, cascadeLevels);
        }
        return m_authorRollbackCascade.get();

    case CascadeLevel::User:
        if (!m_userRollbackCascade) {
            auto cascadeLevels = OptionSet<CascadeLevel> { CascadeLevel::UserAgent };
            m_userRollbackCascade = makeUnique<const PropertyCascade>(*this, cascadeLevels);
        }
        return m_userRollbackCascade.get();

    case CascadeLevel::UserAgent:
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

inline void PropertyCascade::applyProperty(const Property& property)
{
    auto& builderState = *m_builderState;
    builderState.m_cascadeLevel = property.level;
    builderState.m_styleScopeOrdinal = property.styleScopeOrdinal;

    auto applyWithLinkMatch = [&](SelectorChecker::LinkMatchMask linkMatch) {
        if (property.cssValue[linkMatch]) {
            builderState.m_linkMatch = linkMatch;
            applyProperty(property.id, *property.cssValue[linkMatch], linkMatch);
        }
    };

    applyWithLinkMatch(SelectorChecker::MatchDefault);

    if (builderState.style().insideLink() == InsideLink::NotInside)
        return;

    applyWithLinkMatch(SelectorChecker::MatchLink);
    applyWithLinkMatch(SelectorChecker::MatchVisited);

    builderState.m_linkMatch = SelectorChecker::MatchDefault;
}

void PropertyCascade::applyProperty(CSSPropertyID id, CSSValue& value, SelectorChecker::LinkMatchMask linkMatchMask)
{
    ASSERT_WITH_MESSAGE(!isShorthandCSSProperty(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    auto valueToApply = resolveValue(id, value);

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, m_direction, m_writingMode);
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), linkMatchMask);
    }

    auto& builderState = *m_builderState;

    CSSCustomPropertyValue* customPropertyValue = nullptr;
    CSSValueID customPropertyValueID = CSSValueInvalid;
    CSSRegisteredCustomProperty* customPropertyRegistered = nullptr;

    if (id == CSSPropertyCustom) {
        customPropertyValue = downcast<CSSCustomPropertyValue>(valueToApply.ptr());
        ASSERT(customPropertyValue->isResolved());
        if (WTF::holds_alternative<CSSValueID>(customPropertyValue->value()))
            customPropertyValueID = WTF::get<CSSValueID>(customPropertyValue->value());
        auto& name = customPropertyValue->name();
        customPropertyRegistered = builderState.document().getCSSRegisteredCustomPropertySet().get(name);
    }

    bool isInherit = valueToApply->isInheritedValue() || customPropertyValueID == CSSValueInherit;
    bool isInitial = valueToApply->isInitialValue() || customPropertyValueID == CSSValueInitial;

    bool isUnset = valueToApply->isUnsetValue() || customPropertyValueID == CSSValueUnset;
    bool isRevert = valueToApply->isRevertValue() || customPropertyValueID == CSSValueRevert;

    if (isRevert) {
        if (auto* rollback = propertyCascadeForRollback(builderState.m_cascadeLevel)) {
            // With the rollback cascade built, we need to obtain the property and apply it. If the property is
            // not present, then we behave like "unset." Otherwise we apply the property instead of
            // our own.
            if (customPropertyValue) {
                if (customPropertyRegistered && customPropertyRegistered->inherits && rollback->hasCustomProperty(customPropertyValue->name())) {
                    auto property = rollback->customProperty(customPropertyValue->name());
                    if (property.cssValue[linkMatchMask])
                        applyProperty(property.id, *property.cssValue[linkMatchMask], linkMatchMask);
                    return;
                }
            } else if (rollback->hasProperty(id)) {
                auto& property = rollback->property(id);
                if (property.cssValue[linkMatchMask])
                    applyProperty(property.id, *property.cssValue[linkMatchMask], linkMatchMask);
                return;
            }
        }

        isUnset = true;
    }

    if (isUnset) {
        if (CSSProperty::isInheritedProperty(id))
            isInherit = true;
        else
            isInitial = true;
    }

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit

    if (builderState.applyPropertyToVisitedLinkStyle() && !isValidVisitedLinkProperty(id)) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !CSSProperty::isInheritedProperty(id))
        builderState.style().setHasExplicitlyInheritedProperties();

#if ENABLE(CSS_PAINTING_API)
    if (is<CSSPaintImageValue>(valueToApply)) {
        auto& name = downcast<CSSPaintImageValue>(valueToApply.get()).name();
        if (auto* paintWorklet = const_cast<Document&>(builderState.document()).paintWorkletGlobalScopeForName(name)) {
            auto locker = holdLock(paintWorklet->paintDefinitionLock());
            if (auto* registration = paintWorklet->paintDefinitionMap().get(name)) {
                for (auto& property : registration->inputProperties)
                    builderState.style().addCustomPaintWatchProperty(property);
            }
        }
    }
#endif

    // Use the generated StyleBuilder.
    StyleBuilder::applyProperty(id, builderState, valueToApply.get(), isInitial, isInherit, customPropertyRegistered);
}

Ref<CSSValue> PropertyCascade::resolveValue(CSSPropertyID propertyID, CSSValue& value)
{
    if (!value.hasVariableReferences())
        return value;
    
    auto variableValue = resolvedVariableValue(propertyID, value);
    // If the cascade has already applied this id, then we detected a cycle, and this value should be unset.
    if (!variableValue || builderState().m_appliedProperties.get(propertyID)) {
        if (CSSProperty::isInheritedProperty(propertyID))
            return CSSValuePool::singleton().createInheritedValue();
        return CSSValuePool::singleton().createExplicitInitialValue();
    }

    return *variableValue;
}

RefPtr<CSSValue> PropertyCascade::resolvedVariableValue(CSSPropertyID propID, const CSSValue& value)
{
    CSSParser parser(builderState().document());
    return parser.parseValueWithVariableReferences(propID, value, builderState());
}

void PropertyCascade::resolveDirectionAndWritingMode()
{
    auto& style = builderState().style();

    m_direction = style.direction();
    m_writingMode = style.writingMode();

    bool hadImportantWritingMode = false;
    bool hadImportantDirection = false;

    for (auto cascadeLevel : { CascadeLevel::UserAgent, CascadeLevel::User, CascadeLevel::Author }) {
        for (const auto& matchedProperties : declarationsForCascadeLevel(m_matchResult, cascadeLevel)) {
            for (unsigned i = 0, count = matchedProperties.properties->propertyCount(); i < count; ++i) {
                auto property = matchedProperties.properties->propertyAt(i);
                if (!property.value()->isPrimitiveValue())
                    continue;
                switch (property.id()) {
                case CSSPropertyWritingMode:
                    if (!hadImportantWritingMode || property.isImportant()) {
                        m_writingMode = downcast<CSSPrimitiveValue>(*property.value());
                        hadImportantWritingMode = property.isImportant();
                    }
                    break;
                case CSSPropertyDirection:
                    if (!hadImportantDirection || property.isImportant()) {
                        m_direction = downcast<CSSPrimitiveValue>(*property.value());
                        hadImportantDirection = property.isImportant();
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
}

}
}
