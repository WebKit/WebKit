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
#include "PaintWorkletGlobalScope.h"
#include "PropertyAllowlist.h"
#include "StyleBuilderGenerated.h"
#include "StylePropertyShorthand.h"

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
    case CSSPropertyTextDecorationSkipInk:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyTextUnderlineOffset:
    case CSSPropertyTextDecorationThickness:
    case CSSPropertyTextDecoration:
        return true;
    default:
        return false;
    }
}

PropertyCascade::PropertyCascade(const MatchResult& matchResult, CascadeLevel maximumCascadeLevel, IncludedProperties includedProperties, Direction direction)
    : m_matchResult(matchResult)
    , m_includedProperties(includedProperties)
    , m_maximumCascadeLevel(maximumCascadeLevel)
    , m_direction(direction)
{
    buildCascade();
}

PropertyCascade::PropertyCascade(const PropertyCascade& parent, CascadeLevel maximumCascadeLevel, std::optional<CascadeLayerPriority> maximumCascadeLayerPriorityForRollback)
    : m_matchResult(parent.m_matchResult)
    , m_includedProperties(parent.m_includedProperties)
    , m_maximumCascadeLevel(maximumCascadeLevel)
    , m_maximumCascadeLayerPriorityForRollback(maximumCascadeLayerPriorityForRollback)
    , m_direction(parent.direction())
    , m_directionIsUnresolved(false)
{
    buildCascade();
}

PropertyCascade::~PropertyCascade() = default;

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
        property.cssValue[0] = &cssValue;
        property.cssValue[SelectorChecker::MatchLink] = &cssValue;
        property.cssValue[SelectorChecker::MatchVisited] = &cssValue;
    } else
        property.cssValue[matchedProperties.linkMatchType] = &cssValue;
}

void PropertyCascade::set(CSSPropertyID id, CSSValue& cssValue, const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel)
{
    if (CSSProperty::isDirectionAwareProperty(id)) {
        auto direction = this->direction();
        id = CSSProperty::resolveDirectionAwareProperty(id, direction.textDirection, direction.writingMode);
    }

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
            setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
            m_customProperties.set(customValue.name(), property);
        } else {
            Property property = customProperty(customValue.name());
            setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
            m_customProperties.set(customValue.name(), property);
        }
        return;
    }

    if (!m_propertyIsPresent[id])
        memset(property.cssValue, 0, sizeof(property.cssValue));
    m_propertyIsPresent.set(id);
    setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
}

void PropertyCascade::setDeferred(CSSPropertyID id, CSSValue& cssValue, const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(id));
    ASSERT(shouldApplyPropertyInParseOrder(id));

    Property property;
    memset(property.cssValue, 0, sizeof(property.cssValue));
    setPropertyInternal(property, id, cssValue, matchedProperties, cascadeLevel);
    m_deferredProperties.append(property);
}


bool PropertyCascade::addMatch(const MatchedProperties& matchedProperties, CascadeLevel cascadeLevel, bool important)
{
    auto skipForRollback = [&] {
        if (!m_maximumCascadeLayerPriorityForRollback)
            return false;
        if (matchedProperties.styleScopeOrdinal != ScopeOrdinal::Element)
            return false;
        if (cascadeLevel < m_maximumCascadeLevel)
            return false;
        if (matchedProperties.fromStyleAttribute == FromStyleAttribute::Yes)
            return true;
        return matchedProperties.cascadeLayerPriority > *m_maximumCascadeLayerPriorityForRollback;
    };
    if (skipForRollback())
        return false;

    auto& styleProperties = *matchedProperties.properties;
    auto propertyAllowlist = matchedProperties.allowlistType;
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
            ASSERT(!current.value()->isInheritValue());
            continue;
        }
        CSSPropertyID propertyID = current.id();

#if ENABLE(VIDEO)
        if (propertyAllowlist == PropertyAllowlist::Cue && !isValidCueStyleProperty(propertyID))
            continue;
#endif
        if (propertyAllowlist == PropertyAllowlist::Marker && !isValidMarkerStyleProperty(propertyID))
            continue;

        if (shouldApplyPropertyInParseOrder(propertyID))
            setDeferred(propertyID, *current.value(), matchedProperties, cascadeLevel);
        else
            set(propertyID, *current.value(), matchedProperties, cascadeLevel);
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

        if (!hasImportantProperties(*matchedProperties.properties))
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
        addMatch(matchedDeclarations[match.index], cascadeLevel, true);
}

PropertyCascade::Direction PropertyCascade::resolveDirectionAndWritingMode(Direction inheritedDirection) const
{
    Direction result = inheritedDirection;

    bool hadImportantWritingMode = false;
    bool hadImportantDirection = false;

    for (auto cascadeLevel : { CascadeLevel::UserAgent, CascadeLevel::User, CascadeLevel::Author }) {
        for (const auto& matchedProperties : declarationsForCascadeLevel(m_matchResult, cascadeLevel)) {
            for (unsigned i = 0, count = matchedProperties.properties->propertyCount(); i < count; ++i) {
                auto property = matchedProperties.properties->propertyAt(i);
                if (!property.value()->isPrimitiveValue() || property.value()->isCSSWideKeyword())
                    continue;
                switch (property.id()) {
                case CSSPropertyWritingMode:
                    if (!hadImportantWritingMode || property.isImportant()) {
                        result.writingMode = downcast<CSSPrimitiveValue>(*property.value());
                        hadImportantWritingMode = property.isImportant();
                    }
                    break;
                case CSSPropertyDirection:
                    if (!hadImportantDirection || property.isImportant()) {
                        result.textDirection = downcast<CSSPrimitiveValue>(*property.value());
                        hadImportantDirection = property.isImportant();
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    return result;
}

PropertyCascade::Direction PropertyCascade::direction() const
{
    if (m_directionIsUnresolved) {
        m_direction = resolveDirectionAndWritingMode(m_direction);
        m_directionIsUnresolved = false;
    }
    return m_direction;
}

}
}
