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

#include "CSSCustomPropertyValue.h"
#include "CSSPaintImageValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePool.h"
#include "ComputedStyleDependencies.h"
#include "PaintWorkletGlobalScope.h"
#include "PropertyAllowlist.h"
#include "StyleBuilderGenerated.h"
#include "StylePropertyShorthand.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PropertyCascade);

PropertyCascade::PropertyCascade(const MatchResult& matchResult, CascadeLevel maximumCascadeLevel, OptionSet<PropertyType> includedProperties, const HashSet<AnimatableCSSProperty>* animatedProperties)
    : m_matchResult(matchResult)
    , m_includedProperties(includedProperties)
    , m_maximumCascadeLevel(maximumCascadeLevel)
    , m_animationLayer(animatedProperties ? std::optional { AnimationLayer { *animatedProperties } } : std::nullopt)
{
    buildCascade();
}

PropertyCascade::PropertyCascade(const PropertyCascade& parent, CascadeLevel maximumCascadeLevel, std::optional<ScopeOrdinal> rollbackScope, std::optional<CascadeLayerPriority> maximumCascadeLayerPriorityForRollback)
    : m_matchResult(parent.m_matchResult)
    , m_includedProperties(parent.m_includedProperties)
    , m_maximumCascadeLevel(maximumCascadeLevel)
    , m_rollbackScope(rollbackScope)
    , m_maximumCascadeLayerPriorityForRollback(maximumCascadeLayerPriorityForRollback)
    , m_animationLayer(parent.m_animationLayer)
{
    buildCascade();
}

PropertyCascade::~PropertyCascade() = default;

PropertyCascade::AnimationLayer::AnimationLayer(const HashSet<AnimatableCSSProperty>& properties)
    : properties(properties)
{
    hasCustomProperties = std::find_if(properties.begin(), properties.end(), [](auto& property) {
        return std::holds_alternative<AtomString>(property);
    }) != properties.end();

    hasFontSize = properties.contains(CSSPropertyFontSize);
    hasLineHeight = properties.contains(CSSPropertyLineHeight);
}

void PropertyCascade::buildCascade()
{
    OptionSet<CascadeLevel> cascadeLevelsWithImportant;

    for (auto cascadeLevel : { CascadeLevel::UserAgent, CascadeLevel::User, CascadeLevel::Author }) {
        if (cascadeLevel > m_maximumCascadeLevel)
            break;
        bool hasImportant = addNormalMatches(cascadeLevel);
        if (hasImportant)
            cascadeLevelsWithImportant.add(cascadeLevel);
    }

    for (auto cascadeLevel : { CascadeLevel::Author, CascadeLevel::User, CascadeLevel::UserAgent }) {
        if (!cascadeLevelsWithImportant.contains(cascadeLevel))
            continue;
        addImportantMatches(cascadeLevel);
    }

    sortLogicalGroupPropertyIDs();
}

void PropertyCascade::setPropertyInternal(Property& property, CSSPropertyID id, CSSValue& cssValue, const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel)
{
    ASSERT(matchedProperties.linkMatchType <= SelectorChecker::MatchAll);
    property.id = id;
    property.cascadeLevel = cascadeLevel;
    property.styleScopeOrdinal = matchedProperties.styleScopeOrdinal;
    property.cascadeLayerPriority = matchedProperties.cascadeLayerPriority;
    property.fromStyleAttribute = matchedProperties.fromStyleAttribute;

    if (matchedProperties.linkMatchType == SelectorChecker::MatchAll) {
        property.cascadeLevels[SelectorChecker::MatchDefault] = cascadeLevel;
        property.cssValue[SelectorChecker::MatchDefault] = &cssValue;

        property.cascadeLevels[SelectorChecker::MatchLink] = cascadeLevel;
        property.cssValue[SelectorChecker::MatchLink] = &cssValue;

        property.cascadeLevels[SelectorChecker::MatchVisited] = cascadeLevel;
        property.cssValue[SelectorChecker::MatchVisited] = &cssValue;
    } else {
        property.cascadeLevels[matchedProperties.linkMatchType] = cascadeLevel;
        property.cssValue[matchedProperties.linkMatchType] = &cssValue;
    }
}

void PropertyCascade::set(CSSPropertyID id, CSSValue& cssValue, const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel)
{
    ASSERT(!CSSProperty::isInLogicalPropertyGroup(id));
    ASSERT(id < firstLogicalGroupProperty);

    ASSERT(id < m_propertyIsPresent.size());
    if (id == CSSPropertyCustom) {
        m_propertyIsPresent.set(id);
        const auto& customValue = downcast<CSSCustomPropertyValue>(cssValue);
        auto result = m_customProperties.ensure(customValue.name(), [&]() {
            Property property;
            property.cssValue = { };
            setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
            return property;
        });
        if (!result.isNewEntry)
            setPropertyInternal(result.iterator->value, id, cssValue, matchedProperties, cascadeLevel);
        return;
    }

    auto& property = m_properties[id];
    if (!m_propertyIsPresent.testAndSet(id))
        property.cssValue = { };
    setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
}

void PropertyCascade::setLogicalGroupProperty(CSSPropertyID id, CSSValue& cssValue, const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel)
{
    ASSERT(id >= firstLogicalGroupProperty);
    ASSERT(id <= lastLogicalGroupProperty);

    auto& property = m_properties[id];
    if (!hasLogicalGroupProperty(id)) {
        property.cssValue = { };
        m_lowestSeenLogicalGroupProperty = std::min(m_lowestSeenLogicalGroupProperty, id);
        m_highestSeenLogicalGroupProperty = std::max(m_highestSeenLogicalGroupProperty, id);
    }
    setLogicalGroupPropertyIndex(id, ++m_lastIndexForLogicalGroup);
    setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
}

bool PropertyCascade::hasProperty(CSSPropertyID propertyID, const CSSValue& value)
{
    if (propertyID == CSSPropertyCustom)
        return hasCustomProperty(downcast<CSSCustomPropertyValue>(value).name());
    return propertyID < firstLogicalGroupProperty ? hasNormalProperty(propertyID) : hasLogicalGroupProperty(propertyID);
}

const PropertyCascade::Property* PropertyCascade::lastPropertyResolvingLogicalPropertyPair(CSSPropertyID propertyID, WritingMode writingMode) const
{
    ASSERT(CSSProperty::isInLogicalPropertyGroup(propertyID));

    auto pairID = [&] {
        if (CSSProperty::isDirectionAwareProperty(propertyID))
            return CSSProperty::resolveDirectionAwareProperty(propertyID, writingMode);
        return CSSProperty::unresolvePhysicalProperty(propertyID, writingMode);
    }();
    ASSERT(pairID != CSSPropertyInvalid);

    auto indexForPropertyID = logicalGroupPropertyIndex(propertyID);
    auto indexForPairID = logicalGroupPropertyIndex(pairID);
    if (indexForPropertyID > indexForPairID)
        return &logicalGroupProperty(propertyID);
    if (indexForPropertyID < indexForPairID)
        return &logicalGroupProperty(pairID);
    ASSERT(!hasLogicalGroupProperty(propertyID));
    ASSERT(!hasLogicalGroupProperty(pairID));
    return nullptr;
}

bool PropertyCascade::addMatch(const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel, IsImportant important)
{
    auto includePropertiesForRollback = [&] {
        if (m_rollbackScope && matchedProperties.styleScopeOrdinal > *m_rollbackScope)
            return true;
        if (cascadeLevel < m_maximumCascadeLevel)
            return true;
        if (matchedProperties.fromStyleAttribute == FromStyleAttribute::Yes)
            return false;
        return matchedProperties.cascadeLayerPriority <= *m_maximumCascadeLayerPriorityForRollback;
    };
    if (m_maximumCascadeLayerPriorityForRollback && !includePropertiesForRollback())
        return false;

    if (matchedProperties.isStartingStyle == IsStartingStyle::Yes && !m_includedProperties.contains(PropertyType::StartingStyle))
        return false;

    auto propertyAllowlist = matchedProperties.allowlistType;
    bool hasImportantProperties = false;

    for (auto current : matchedProperties.properties.get()) {
        if (current.isImportant())
            hasImportantProperties = true;
        if ((important == IsImportant::No && current.isImportant()) || (important == IsImportant::Yes && !current.isImportant()))
            continue;

        auto propertyID = cascadeAliasProperty(current.id());

        auto shouldIncludeProperty = [&] {
#if ENABLE(VIDEO)
            if (propertyAllowlist == PropertyAllowlist::Cue && !isValidCueStyleProperty(propertyID))
                return false;
            if (propertyAllowlist == PropertyAllowlist::CueSelector && !isValidCueSelectorStyleProperty(propertyID))
                return false;
            if (propertyAllowlist == PropertyAllowlist::CueBackground && !isValidCueBackgroundStyleProperty(propertyID))
                return false;
#endif
            if (propertyAllowlist == PropertyAllowlist::Marker && !isValidMarkerStyleProperty(propertyID))
                return false;

            if (m_includedProperties.containsAll(normalProperties()))
                return true;

            if (matchedProperties.isCacheable == IsCacheable::Partially && m_includedProperties.contains(PropertyType::NonCacheable))
                return true;

            // If we have applied this property for some reason already we must apply anything that overrides it.
            if (hasProperty(propertyID, *current.value()))
                return true;

            if (m_includedProperties.containsAny({ PropertyType::AfterAnimation, PropertyType::AfterTransition })) {
                if (shouldApplyAfterAnimation(current)) {
                    m_animationLayer->overriddenProperties.add(propertyID);
                    return true;
                }
                return false;
            }

            if (m_includedProperties.contains(PropertyType::Inherited) && current.isInherited())
                return true;
            if (m_includedProperties.contains(PropertyType::ExplicitlyInherited) && isValueID(*current.value(), CSSValueInherit))
                return true;
            if (m_includedProperties.contains(PropertyType::NonInherited) && !current.isInherited())
                return true;

            // Apply all logical group properties if we have applied any. They may override the ones we already applied.
            if (propertyID >= firstLogicalGroupProperty && m_lastIndexForLogicalGroup)
                return true;

            return false;
        }();

        if (!shouldIncludeProperty)
            continue;

        if (propertyID < firstLogicalGroupProperty)
            set(propertyID, *current.value(), matchedProperties, cascadeLevel);
        else
            setLogicalGroupProperty(propertyID, *current.value(), matchedProperties, cascadeLevel);
    }

    return hasImportantProperties;
}

bool PropertyCascade::shouldApplyAfterAnimation(const StyleProperties::PropertyReference& property)
{
    ASSERT(m_animationLayer);

    auto id = property.id();
    auto* customProperty = dynamicDowncast<CSSCustomPropertyValue>(*property.value());

    auto isAnimatedProperty = [&] {
        if (customProperty)
            return m_animationLayer->properties.contains(customProperty->name());
        return m_animationLayer->properties.contains(id);
    }();

    if (isAnimatedProperty) {
        // "Important declarations from all origins take precedence over animations."
        // https://drafts.csswg.org/css-cascade-5/#importance
        return m_includedProperties.contains(PropertyType::AfterAnimation) && property.isImportant();
    }

    // If we are animating custom properties they may affect other properties so we need to re-resolve them.
    if (m_animationLayer->hasCustomProperties) {
        // We could check if the we are actually animating the referenced variable. Indirect cases would need to be taken into account.
        if (customProperty && !customProperty->isResolved())
            return true;
        if (property.value()->hasVariableReferences())
            return true;
    }

    // Check for 'em' units and similar property dependencies.
    if (m_animationLayer->hasFontSize || m_animationLayer->hasLineHeight) {
        auto dependencies = property.value()->computedStyleDependencies();
        if (m_animationLayer->hasFontSize && dependencies.properties.contains(CSSPropertyFontSize))
            return true;
        if (m_animationLayer->hasLineHeight && dependencies.properties.contains(CSSPropertyLineHeight))
            return true;
    }

    return false;
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
        hasImportant |= addMatch(matchedDeclarations, cascadeLevel, IsImportant::No);

    return hasImportant;
}

static bool hasImportantProperties(const StyleProperties& properties)
{
    for (auto property : properties) {
        if (property.isImportant())
            return true;
    }
    return false;
}

void PropertyCascade::addImportantMatches(CascadeLevel cascadeLevel)
{
    struct ImportantMatch {
        unsigned index;
        ScopeOrdinal ordinal;
        CascadeLayerPriority layerPriority;
        FromStyleAttribute fromStyleAttribute;
    };
    Vector<ImportantMatch> importantMatches;
    bool hasMatchesFromOtherScopesOrLayers = false;

    auto& matchedDeclarations = declarationsForCascadeLevel(m_matchResult, cascadeLevel);

    for (unsigned i = 0; i < matchedDeclarations.size(); ++i) {
        const MatchedProperties& matchedProperties = matchedDeclarations[i];

        if (!hasImportantProperties(matchedProperties.properties))
            continue;

        importantMatches.append({ i, matchedProperties.styleScopeOrdinal, matchedProperties.cascadeLayerPriority, matchedProperties.fromStyleAttribute });

        if (matchedProperties.styleScopeOrdinal != ScopeOrdinal::Element || matchedProperties.cascadeLayerPriority != RuleSet::cascadeLayerPriorityForUnlayered)
            hasMatchesFromOtherScopesOrLayers = true;
    }

    if (importantMatches.isEmpty())
        return;

    if (hasMatchesFromOtherScopesOrLayers) {
        // Match results are sorted in reverse tree context order so this is not needed for normal properties.
        std::stable_sort(importantMatches.begin(), importantMatches.end(), [] (auto& a, auto& b) {
            // For !important properties a later shadow tree wins.
            if (a.ordinal != b.ordinal)
                return a.ordinal < b.ordinal;
            // Lower priority layer wins, except if style attribute is involved.
            if (a.fromStyleAttribute != b.fromStyleAttribute)
                return a.fromStyleAttribute == FromStyleAttribute::No;
            return a.layerPriority > b.layerPriority;
        });
    }

    for (auto& match : importantMatches)
        addMatch(matchedDeclarations[match.index], cascadeLevel, IsImportant::Yes);
}

void PropertyCascade::sortLogicalGroupPropertyIDs()
{
    size_t endIndex = 0;
    for (uint16_t id = m_lowestSeenLogicalGroupProperty; id <= m_highestSeenLogicalGroupProperty; ++id) {
        auto propertyID = static_cast<CSSPropertyID>(id);
        if (hasLogicalGroupProperty(propertyID))
            m_logicalGroupPropertyIDs[endIndex++] = propertyID;
    }
    m_seenLogicalGroupPropertyCount = endIndex;
    auto logicalGroupPropertyIDs = std::span { m_logicalGroupPropertyIDs }.first(endIndex);
    std::sort(logicalGroupPropertyIDs.begin(), logicalGroupPropertyIDs.end(), [&](auto id1, auto id2) {
        return logicalGroupPropertyIndex(id1) < logicalGroupPropertyIndex(id2);
    });
}

const HashSet<AnimatableCSSProperty> PropertyCascade::overriddenAnimatedProperties() const
{
    if (m_animationLayer)
        return m_animationLayer->overriddenProperties;
    return { };
}

}
}
