/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "StyleBuilder.h"

#include "CSSFontSelector.h"
#include "CSSPaintImageValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSValuePool.h"
#include "CustomPropertyRegistry.h"
#include "Document.h"
#include "HTMLElement.h"
#include "PaintWorkletGlobalScope.h"
#include "Settings.h"
#include "StyleBuilderGenerated.h"
#include "StyleFontSizeFunctions.h"
#include "StylePropertyShorthand.h"

#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static const CSSPropertyID firstLowPriorityProperty = static_cast<CSSPropertyID>(lastHighPriorityProperty + 1);

inline bool isValidVisitedLinkProperty(CSSPropertyID id)
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
    case CSSPropertyTextEmphasisColor:
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

Builder::Builder(RenderStyle& style, BuilderContext&& context, const MatchResult& matchResult, CascadeLevel cascadeLevel, PropertyCascade::IncludedProperties includedProperties)
    : m_cascade(matchResult, cascadeLevel, includedProperties)
    , m_state(*this, style, WTFMove(context))
{
}

Builder::~Builder() = default;

void Builder::applyAllProperties()
{
    applyTopPriorityProperties();
    applyHighPriorityProperties();
    applyNonHighPriorityProperties();
}

// Top priority properties affect resolution of high priority properties.
void Builder::applyTopPriorityProperties()
{
    applyProperties(firstTopPriorityProperty, lastTopPriorityProperty);
    m_state.adjustStyleForInterCharacterRuby();
}

// High priority properties may affect resolution of other properties (they are mostly font related).
void Builder::applyHighPriorityProperties()
{
    applyProperties(firstHighPriorityProperty, lastHighPriorityProperty);
    m_state.updateFont();
}

void Builder::applyNonHighPriorityProperties()
{
    ASSERT(!m_state.fontDirty());

    applyProperties(firstLowPriorityProperty, lastLowPriorityProperty);
    applyDeferredProperties();
    // Any referenced custom properties are already resolved. This will resolve the remaining ones.
    applyCustomProperties();

    ASSERT(!m_state.fontDirty());
}

void Builder::applyDeferredProperties()
{
    for (auto id : m_cascade.deferredPropertyIDs())
        applyCascadeProperty(m_cascade.deferredProperty(id));
}

void Builder::applyProperties(int firstProperty, int lastProperty)
{
    if (LIKELY(m_cascade.customProperties().isEmpty()))
        return applyPropertiesImpl<CustomPropertyCycleTracking::Disabled>(firstProperty, lastProperty);

    return applyPropertiesImpl<CustomPropertyCycleTracking::Enabled>(firstProperty, lastProperty);
}

template<Builder::CustomPropertyCycleTracking trackCycles>
inline void Builder::applyPropertiesImpl(int firstProperty, int lastProperty)
{
    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!m_cascade.hasNormalProperty(propertyID))
            continue;
        ASSERT(propertyID != CSSPropertyCustom);
        auto& property = m_cascade.normalProperty(propertyID);

        if (trackCycles == CustomPropertyCycleTracking::Enabled) {
            m_state.m_inProgressProperties.set(propertyID);
            applyCascadeProperty(property);
            m_state.m_inProgressProperties.clear(propertyID);
            continue;
        }

        // If we don't have any custom properties, then there can't be any cycles.
        applyCascadeProperty(property);
    }
}

void Builder::applyCustomProperties()
{
    for (auto& name : m_cascade.customProperties().keys())
        applyCustomProperty(name);
}

void Builder::applyCustomProperty(const AtomString& name)
{
    if (m_state.m_appliedCustomProperties.contains(name) || !m_cascade.customProperties().contains(name))
        return;

    auto& property = m_cascade.customProperty(name);
    if (!property.cssValue[SelectorChecker::MatchDefault])
        return;

    SetForScope levelScope(m_state.m_currentProperty, &property);

    Ref customPropertyValue = downcast<CSSCustomPropertyValue>(*property.cssValue[SelectorChecker::MatchDefault]);

    bool inCycle = !m_state.m_inProgressCustomProperties.add(name).isNewEntry;

    auto createInvalidOrUnset = [&] {
        // https://drafts.csswg.org/css-variables-2/#invalid-variables
        auto* registered = m_state.document().customPropertyRegistry().get(name);
        // The property is a non-registered custom property:
        // The property is a registered custom property with universal syntax:
        // The computed value is the guaranteed-invalid value.
        if (!registered || registered->syntax.isUniversal())
            return CSSCustomPropertyValue::createWithID(name, CSSValueInvalid);
        // Otherwise:
        // ...as if the property’s value had been specified as the unset keyword.
        return CSSCustomPropertyValue::createWithID(name, CSSValueUnset);
    };

    auto valueToApply = [&]() -> RefPtr<CSSCustomPropertyValue> {
        if (inCycle)
            return createInvalidOrUnset();

        auto resolvedValue = resolveCustomPropertyValueWithVariableReferences(customPropertyValue.get());
        if (m_state.m_appliedCustomProperties.contains(name))
            return nullptr; // There was a cycle and the value was already resolved, so bail.

        if (!resolvedValue)
            return createInvalidOrUnset();

        return resolvedValue;
    }();

    if (!valueToApply) {
        ASSERT(!m_state.m_inProgressCustomProperties.contains(name));
        return;
    }

    SetForScope scopedLinkMatchMutation(m_state.m_linkMatch, SelectorChecker::MatchDefault);
    applyProperty(CSSPropertyCustom, *valueToApply, SelectorChecker::MatchDefault);

    m_state.m_inProgressCustomProperties.remove(name);
    m_state.m_appliedCustomProperties.add(name);

    if (inCycle) {
        // Resolve this value so that we reset its dependencies.
        resolveCustomPropertyValueWithVariableReferences(customPropertyValue.get());
    }
}

inline void Builder::applyCascadeProperty(const PropertyCascade::Property& property)
{
    SetForScope levelScope(m_state.m_currentProperty, &property);

    auto applyWithLinkMatch = [&](SelectorChecker::LinkMatchMask linkMatch) {
        if (property.cssValue[linkMatch]) {
            SetForScope scopedLinkMatchMutation(m_state.m_linkMatch, linkMatch);
            applyProperty(property.id, *property.cssValue[linkMatch], linkMatch);
        }
    };

    applyWithLinkMatch(SelectorChecker::MatchDefault);

    if (m_state.style().insideLink() == InsideLink::NotInside)
        return;

    applyWithLinkMatch(SelectorChecker::MatchLink);
    applyWithLinkMatch(SelectorChecker::MatchVisited);

    m_state.m_linkMatch = SelectorChecker::MatchDefault;
}

void Builder::applyRollbackCascadeProperty(const PropertyCascade::Property& property, SelectorChecker::LinkMatchMask linkMatchMask)
{
    auto* value = property.cssValue[linkMatchMask];
    if (!value)
        return;

    SetForScope levelScope(m_state.m_currentProperty, &property);

    applyProperty(property.id, *value, linkMatchMask);
}

void Builder::applyProperty(CSSPropertyID id, CSSValue& value, SelectorChecker::LinkMatchMask linkMatchMask)
{
    ASSERT_WITH_MESSAGE(!isShorthandCSSProperty(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    auto valueToApply = resolveVariableReferences(id, value);
    auto& style = m_state.style();

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, style.direction(), style.writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), linkMatchMask);
    }

    const CSSCustomPropertyValue* customPropertyValue = nullptr;
    CSSValueID customPropertyValueID = CSSValueInvalid;
    const CSSRegisteredCustomProperty* registeredCustomProperty = nullptr;

    if (id == CSSPropertyCustom) {
        customPropertyValue = downcast<CSSCustomPropertyValue>(valueToApply.ptr());
        ASSERT(customPropertyValue->isResolved());
        if (std::holds_alternative<CSSValueID>(customPropertyValue->value()))
            customPropertyValueID = std::get<CSSValueID>(customPropertyValue->value());
        auto& name = customPropertyValue->name();
        registeredCustomProperty = m_state.document().customPropertyRegistry().get(name);
    }

    bool isInherit = valueToApply->isInheritValue() || customPropertyValueID == CSSValueInherit;
    bool isInitial = valueToApply->isInitialValue() || customPropertyValueID == CSSValueInitial;

    bool isUnset = valueToApply->isUnsetValue() || customPropertyValueID == CSSValueUnset;
    bool isRevert = valueToApply->isRevertValue() || customPropertyValueID == CSSValueRevert;
    bool isRevertLayer = valueToApply->isRevertLayerValue() || customPropertyValueID == CSSValueRevertLayer;

    if (isRevert || isRevertLayer) {
        // In @keyframes, 'revert-layer' rolls back the cascaded value to the author level.
        // We can just not apply the property in order to keep the value from the base style.
        if (isRevertLayer && m_state.m_isBuildingKeyframeStyle)
            return;

        auto* rollbackCascade = isRevert ? ensureRollbackCascadeForRevert() : ensureRollbackCascadeForRevertLayer();

        if (rollbackCascade) {
            // With the rollback cascade built, we need to obtain the property and apply it. If the property is
            // not present, then we behave like "unset." Otherwise we apply the property instead of our own.
            if (customPropertyValue) {
                if (registeredCustomProperty && registeredCustomProperty->inherits && rollbackCascade->hasCustomProperty(customPropertyValue->name())) {
                    auto property = rollbackCascade->customProperty(customPropertyValue->name());
                    applyRollbackCascadeProperty(property, linkMatchMask);
                    return;
                }
            } else if (id < firstDeferredProperty) {
                if (rollbackCascade->hasNormalProperty(id)) {
                    auto& property = rollbackCascade->normalProperty(id);
                    applyRollbackCascadeProperty(property, linkMatchMask);
                    return;
                }
            } else if (auto* property = rollbackCascade->lastDeferredPropertyResolvingRelated(id, style.direction(), style.writingMode())) {
                applyRollbackCascadeProperty(*property, linkMatchMask);
                return;
            }
        }

        isUnset = true;
    }

    auto isInheritedProperty = [&] {
        return registeredCustomProperty ? registeredCustomProperty->inherits : CSSProperty::isInheritedProperty(id);
    };

    if (isUnset) {
        // https://drafts.csswg.org/css-cascade-4/#inherit-initial
        // The unset CSS-wide keyword acts as either inherit or initial, depending on whether the property is inherited or not.
        if (isInheritedProperty())
            isInherit = true;
        else
            isInitial = true;
    }

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit

    if (m_state.applyPropertyToVisitedLinkStyle() && !isValidVisitedLinkProperty(id)) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !isInheritedProperty())
        style.setHasExplicitlyInheritedProperties();

#if ENABLE(CSS_PAINTING_API)
    if (is<CSSPaintImageValue>(valueToApply)) {
        auto& name = downcast<CSSPaintImageValue>(valueToApply.get()).name();
        if (auto* paintWorklet = const_cast<Document&>(m_state.document()).paintWorkletGlobalScopeForName(name)) {
            Locker locker { paintWorklet->paintDefinitionLock() };
            if (auto* registration = paintWorklet->paintDefinitionMap().get(name)) {
                for (auto& property : registration->inputProperties)
                    style.addCustomPaintWatchProperty(property);
            }
        }
    }
#endif

    BuilderGenerated::applyProperty(id, m_state, valueToApply.get(), isInitial, isInherit, registeredCustomProperty);
}

Ref<CSSValue> Builder::resolveVariableReferences(CSSPropertyID propertyID, CSSValue& value)
{
    if (!value.hasVariableReferences())
        return value;

    auto variableValue = CSSParser { m_state.document() }.parseValueWithVariableReferences(propertyID, value, m_state);

    // https://drafts.csswg.org/css-variables-2/#invalid-variables
    // ...as if the property’s value had been specified as the unset keyword.
    if (!variableValue || m_state.m_inUnitCycleProperties.get(propertyID))
        return CSSValuePool::singleton().createValue(CSSValueUnset);

    return *variableValue;
}

RefPtr<CSSCustomPropertyValue> Builder::resolveCustomPropertyValueWithVariableReferences(CSSCustomPropertyValue& value)
{
    if (!std::holds_alternative<Ref<CSSVariableReferenceValue>>(value.value()))
        return &value;
    return CSSParser { m_state.document() }.parseCustomPropertyValueWithVariableReferences(value, m_state);
}

const PropertyCascade* Builder::ensureRollbackCascadeForRevert()
{
    auto rollbackCascadeLevel = m_state.m_currentProperty->cascadeLevel;
    if (rollbackCascadeLevel == CascadeLevel::UserAgent)
        return nullptr;

    --rollbackCascadeLevel;

    auto key = makeRollbackCascadeKey(rollbackCascadeLevel);
    return m_rollbackCascades.ensure(key, [&] {
        return makeUnique<const PropertyCascade>(m_cascade, rollbackCascadeLevel);
    }).iterator->value.get();
}

const PropertyCascade* Builder::ensureRollbackCascadeForRevertLayer()
{
    auto& property = *m_state.m_currentProperty;
    auto rollbackLayerPriority = property.cascadeLayerPriority;
    if (!rollbackLayerPriority)
        return ensureRollbackCascadeForRevert();

    ASSERT(property.fromStyleAttribute == FromStyleAttribute::No || property.cascadeLayerPriority == RuleSet::cascadeLayerPriorityForUnlayered);

    // Style attribute reverts to the regular author style.
    if (property.fromStyleAttribute == FromStyleAttribute::No)
        --rollbackLayerPriority;

    auto key = makeRollbackCascadeKey(property.cascadeLevel, property.styleScopeOrdinal, rollbackLayerPriority);
    return m_rollbackCascades.ensure(key, [&] {
        return makeUnique<const PropertyCascade>(m_cascade, property.cascadeLevel, property.styleScopeOrdinal, rollbackLayerPriority);
    }).iterator->value.get();
}

auto Builder::makeRollbackCascadeKey(CascadeLevel cascadeLevel, ScopeOrdinal scopeOrdinal, CascadeLayerPriority cascadeLayerPriority) -> RollbackCascadeKey
{
    return { static_cast<unsigned>(cascadeLevel), static_cast<unsigned>(scopeOrdinal), static_cast<unsigned>(cascadeLayerPriority) };
}

}
}
