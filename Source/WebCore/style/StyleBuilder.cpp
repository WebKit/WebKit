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

#include "CSSCustomPropertyValue.h"
#include "CSSFontSelector.h"
#include "CSSPaintImageValue.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CSSWideKeyword.h"
#include "ComputedStyleDependencies.h"
#include "Document.h"
#include "HTMLElement.h"
#include "PaintWorkletGlobalScope.h"
#include "RenderStyleSetters.h"
#include "Settings.h"
#include "StyleAdjuster.h"
#include "StyleBuilderGenerated.h"
#include "StyleCustomProperty.h"
#include "StyleCustomPropertyData.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleFontSizeFunctions.h"
#include "StylePropertyShorthand.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Builder);

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

static auto positionTryFallbackProperties(const BuilderContext& context)
{
    return context.positionTryFallback ? context.positionTryFallback->properties.get() : nullptr;
}

Builder::Builder(RenderStyle& style, BuilderContext&& context, const MatchResult& matchResult, PropertyCascade::IncludedProperties&& includedProperties, const HashSet<AnimatableCSSProperty>* animatedPropertes)
    : m_cascade(matchResult, WTFMove(includedProperties), animatedPropertes, positionTryFallbackProperties(context))
    , m_state(BuilderState::create(style, WTFMove(context)))
{
}

Builder::~Builder() = default;

void Builder::applyAllProperties()
{
    if (m_cascade.isEmpty())
        return;

    applyTopPriorityProperties();
    applyHighPriorityProperties();
    applyNonHighPriorityProperties();

    adjustAfterApplying();
}

// Top priority properties affect resolution of high priority properties.
void Builder::applyTopPriorityProperties()
{
    if (m_cascade.applyLowPriorityOnly())
        return;

    applyProperties(firstTopPriorityProperty, lastTopPriorityProperty);
    m_state->adjustStyleForInterCharacterRuby();
}

// High priority properties may affect resolution of other properties (they are mostly font related).
void Builder::applyHighPriorityProperties()
{
    if (m_cascade.applyLowPriorityOnly())
        return;

    applyProperties(firstHighPriorityProperty, lastHighPriorityProperty);
    m_state->updateFont();
    // This needs to apply before other properties for the `lh` unit, but after updating the font.
    applyProperties(CSSPropertyLineHeight, CSSPropertyLineHeight);
}

void Builder::applyNonHighPriorityProperties()
{
    ASSERT(!m_state->fontDirty());

    applyProperties(firstLowPriorityProperty, lastLowPriorityProperty);
    applyLogicalGroupProperties();
    // Any referenced custom properties are already resolved. This will resolve the remaining ones.
    applyCustomProperties();

    ASSERT(!m_state->fontDirty());
}

void Builder::adjustAfterApplying()
{
    Adjuster::adjustFromBuilder(m_state->style());
}

void Builder::applyLogicalGroupProperties()
{
    // Properties in a logical property group are applied in author specified order which is maintained separately for them.
    for (auto id : m_cascade.logicalGroupPropertyIDs())
        applyCascadeProperty(m_cascade.logicalGroupProperty(id));
}

void Builder::applyProperties(int firstProperty, int lastProperty)
{
    if (m_cascade.customProperties().isEmpty()) [[likely]]
        return applyPropertiesImpl<CustomPropertyCycleTracking::Disabled>(firstProperty, lastProperty);

    return applyPropertiesImpl<CustomPropertyCycleTracking::Enabled>(firstProperty, lastProperty);
}

template<Builder::CustomPropertyCycleTracking trackCycles>
inline void Builder::applyPropertiesImpl(int firstProperty, int lastProperty)
{
    auto applyProperty = [&](size_t index) ALWAYS_INLINE_LAMBDA {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(index);
        ASSERT(propertyID != CSSPropertyCustom);
        auto& property = m_cascade.normalProperty(propertyID);

        if constexpr (trackCycles == CustomPropertyCycleTracking::Enabled) {
            m_state->m_inProgressProperties.set(propertyID);
            applyCascadeProperty(property);
            m_state->m_inProgressProperties.clear(propertyID);
            return;
        }

        // If we don't have any custom properties, then there can't be any cycles.
        applyCascadeProperty(property);
    };

    if (m_cascade.propertyIsPresent().size() == static_cast<size_t>(lastProperty + 1)) {
        m_cascade.propertyIsPresent().forEachSetBit(firstProperty, applyProperty);
        return;
    }

    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!m_cascade.hasNormalProperty(propertyID))
            continue;
        applyProperty(id);
    }
}

void Builder::applyCustomProperties()
{
    for (auto& [name, value] : m_cascade.customProperties()) {
        if (m_state->m_appliedCustomProperties.contains(name))
            continue;
        applyCustomPropertyImpl(name, value);
    }
}

void Builder::applyCustomProperty(const AtomString& name)
{
    if (m_state->m_appliedCustomProperties.contains(name))
        return;

    auto iterator = m_cascade.customProperties().find(name);
    if (iterator == m_cascade.customProperties().end())
        return;

    applyCustomPropertyImpl(name, iterator->value);
}

void Builder::applyCustomPropertyImpl(const AtomString& name, const PropertyCascade::Property& property)
{
    if (!property.cssValue[SelectorChecker::MatchDefault])
        return;

    Ref customPropertyValue = downcast<CSSCustomPropertyValue>(*property.cssValue[SelectorChecker::MatchDefault]);

    bool inCycle = !m_state->m_inProgressCustomProperties.add(name).isNewEntry;
    if (inCycle) {
        auto isNewCycle = m_state->m_inCycleCustomProperties.add(name).isNewEntry;
        if (isNewCycle) {
            // Continue resolving dependencies so we detect cycles for them as well.
            resolveCustomPropertyValue(customPropertyValue.get());
        }
        return;
    }

    // There may be multiple cycles through the same property. Avoid interference from any previously detected cycles.
    auto savedInCycleProperties = std::exchange(m_state->m_inCycleCustomProperties, { });

    auto createInvalidOrUnset = [&] -> Variant<Ref<const Style::CustomProperty>, CSSWideKeyword> {
        // https://drafts.csswg.org/css-variables-2/#invalid-variables
        auto* registered = m_state->document().customPropertyRegistry().get(name);
        // The property is a non-registered custom property:
        // The property is a registered custom property with universal syntax:
        // The computed value is the guaranteed-invalid value.
        if (!registered || registered->syntax.isUniversal())
            return CustomProperty::createForGuaranteedInvalid(name);
        // Otherwise:
        // ...as if the property’s value had been specified as the unset keyword.
        return CSSWideKeyword::Unset;
    };

    auto resolvedValue = resolveCustomPropertyValue(customPropertyValue.get());

    if (!resolvedValue || m_state->m_inCycleCustomProperties.contains(name))
        resolvedValue = createInvalidOrUnset();

    SetForScope levelScope(m_state->m_currentProperty, &property);
    SetForScope scopedLinkMatchMutation(m_state->m_linkMatch, SelectorChecker::MatchDefault);
    applyCustomProperty(name, WTFMove(*resolvedValue));

    AtomString takenName = m_state->m_inProgressCustomProperties.take(name);
    m_state->m_appliedCustomProperties.add(WTFMove(takenName));
    m_state->m_inCycleCustomProperties.addAll(WTFMove(savedInCycleProperties));
}

inline void Builder::applyCascadeProperty(const PropertyCascade::Property& property)
{
    SetForScope levelScope(m_state->m_currentProperty, &property);

    auto applyWithLinkMatch = [&](SelectorChecker::LinkMatchMask linkMatch) {
        if (property.cssValue[linkMatch]) {
            SetForScope scopedLinkMatchMutation(m_state->m_linkMatch, linkMatch);
            applyProperty(property.id, *property.cssValue[linkMatch], linkMatch, property.origins[linkMatch]);
        }
    };

    applyWithLinkMatch(SelectorChecker::MatchDefault);

    if (m_state->style().insideLink() == InsideLink::NotInside)
        return;

    applyWithLinkMatch(SelectorChecker::MatchLink);
    applyWithLinkMatch(SelectorChecker::MatchVisited);

    m_state->m_linkMatch = SelectorChecker::MatchDefault;
}

bool Builder::applyRollbackCascadeProperty(const PropertyCascade& rollbackCascade, CSSPropertyID propertyID, SelectorChecker::LinkMatchMask linkMatchMask)
{
    ASSERT(propertyID != CSSPropertyCustom);

    auto* rollbackProperty = [&]() -> const PropertyCascade::Property* {
        if (propertyID < firstLogicalGroupProperty) {
            if (rollbackCascade.hasNormalProperty(propertyID))
                return &rollbackCascade.normalProperty(propertyID);
            return nullptr;
        }
        return rollbackCascade.lastPropertyResolvingLogicalPropertyPair(propertyID, m_state->style().writingMode());
    }();

    if (!rollbackProperty)
        return false;

    if (auto* value = rollbackProperty->cssValue[linkMatchMask]) {
        SetForScope levelScope(m_state->m_currentProperty, rollbackProperty);
        applyProperty(propertyID, *value, linkMatchMask, rollbackProperty->origin);
    }
    return true;
}

bool Builder::applyRollbackCascadeCustomProperty(const PropertyCascade& rollbackCascade, const AtomString& name)
{
    auto iterator = rollbackCascade.customProperties().find(name);
    if (iterator == rollbackCascade.customProperties().end())
        return false;

    auto& rollbackProperty = iterator->value;
    if (auto* value = rollbackProperty.cssValue[SelectorChecker::MatchDefault]) {
        Ref customPropertyValue = downcast<CSSCustomPropertyValue>(*value);

        SetForScope levelScope(m_state->m_currentProperty, &rollbackProperty);
        auto resolvedValue = resolveCustomPropertyValue(customPropertyValue);
        if (!resolvedValue)
            resolvedValue = CustomProperty::createForGuaranteedInvalid(name);

        applyCustomProperty(name, WTFMove(*resolvedValue));
    }
    return true;
}

void Builder::applyProperty(CSSPropertyID id, CSSValue& value, SelectorChecker::LinkMatchMask linkMatchMask, PropertyCascade::Origin cascadeOrigin)
{
    ASSERT_WITH_MESSAGE(!isShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);
    ASSERT_WITH_MESSAGE(id != CSSPropertyCustom, "Custom property should be handled by applyCustomProperty");

    auto valueToApply = resolveInternalAutoBaseFunction(value);
    valueToApply = resolveVariableReferences(id, valueToApply);
    auto& style = m_state->style();

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, style.writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), linkMatchMask, cascadeOrigin);
    }

    if (m_state->positionTryFallback())
        id = AnchorPositionEvaluator::resolvePositionTryFallbackProperty(id, style.writingMode(), *m_state->positionTryFallback());

    auto valueID = WebCore::valueID(valueToApply.get());
    auto valueType = [&] {
        if (valueID == CSSValueInherit)
            return ApplyValueType::Inherit;
        if (valueID == CSSValueInitial)
            return ApplyValueType::Initial;
        return ApplyValueType::Value;
    }();

    bool isUnset = valueID == CSSValueUnset;
    bool isRevert = valueID == CSSValueRevert;
    bool isRevertLayer = valueID == CSSValueRevertLayer;
    bool isRevertOrRevertLayer = isRevert || isRevertLayer;

    if (isRevertOrRevertLayer) {
        // In @keyframes, 'revert-layer' rolls back the cascaded value to the author level.
        // We can just not apply the property in order to keep the value from the base style.
        if (isRevertLayer && m_state->m_isBuildingKeyframeStyle)
            return;

        auto* rollbackCascade = isRevert ? ensureRollbackCascadeForRevert() : ensureRollbackCascadeForRevertLayer();

        if (rollbackCascade) {
            // With the rollback cascade built, we need to obtain the property and apply it. If the property is
            // not present, then we behave like "unset." Otherwise we apply the property instead of our own.
            if (applyRollbackCascadeProperty(*rollbackCascade, id, linkMatchMask))
                return;
        }
    }

    auto isInheritedProperty = [&] {
        return CSSProperty::isInheritedProperty(id);
    };

    auto unsetValueType = [&] {
        // https://drafts.csswg.org/css-cascade-4/#inherit-initial
        // The unset CSS-wide keyword acts as either inherit or initial, depending on whether the property is inherited or not.
        return isInheritedProperty() ? ApplyValueType::Inherit : ApplyValueType::Initial;
    };

    if (isUnset || isRevertOrRevertLayer)
        valueType = unsetValueType();

    if (!m_state->applyPropertyToRegularStyle() && !isValidVisitedLinkProperty(id)) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (valueType == ApplyValueType::Inherit && !isInheritedProperty())
        style.setHasExplicitlyInheritedProperties();

    if (auto* paintImageValue = dynamicDowncast<CSSPaintImageValue>(valueToApply.get())) {
        auto& name = paintImageValue->name();
        if (auto* paintWorklet = const_cast<Document&>(m_state->document()).paintWorkletGlobalScopeForName(name)) {
            Locker locker { paintWorklet->paintDefinitionLock() };
            if (auto* registration = paintWorklet->paintDefinitionMap().get(name)) {
                for (auto& property : registration->inputProperties)
                    style.addCustomPaintWatchProperty(property);
            }
        }
    }

    if (id == CSSPropertySize && valueType == ApplyValueType::Value) [[unlikely]] {
        applyPageSizeDescriptor(valueToApply.get());
        return;
    }

    BuilderGenerated::applyProperty(id, m_state, valueToApply.get(), valueType);

    if (!isRevertOrRevertLayer)
        m_state->disableNativeAppearanceIfNeeded(id, cascadeOrigin);

    if (!isUnset && !isRevertOrRevertLayer && m_state->isCurrentPropertyInvalidAtComputedValueTime()) {
        // https://drafts.csswg.org/css-variables-2/#invalid-variables
        // A declaration can be invalid at computed-value time if...
        // When this happens, the computed value is one of the following...
        // Otherwise: Either the property’s inherited value or its initial value depending on whether the property
        // is inherited or not, respectively, as if the property’s value had been specified as the unset keyword
        BuilderGenerated::applyProperty(id, m_state, valueToApply.get(), unsetValueType());
    }
}

void Builder::applyCustomProperty(const AtomString& name, Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>&& parsedCustomProperty)
{
    auto& style = m_state->style();

    auto registeredCustomProperty = m_state->document().customPropertyRegistry().get(name);

    auto applyValue = [&](Ref<const CustomProperty>&& valueToApply) {
        bool isInherited = !registeredCustomProperty || registeredCustomProperty->inherits;
        state().style().setCustomPropertyValue(WTFMove(valueToApply), isInherited);
    };

    auto applyInitial = [&] {
        if (registeredCustomProperty && registeredCustomProperty->initialValue) {
            applyValue(*registeredCustomProperty->initialValue);
            return;
        }
        applyValue(CustomProperty::createForGuaranteedInvalid(name));
    };

    auto applyInherit = [&] {
        auto* parentValue = state().parentStyle().inheritedCustomProperties().get(name);
        if (parentValue && !(registeredCustomProperty && !registeredCustomProperty->inherits)) {
            applyValue(*parentValue);
            return;
        }
        if (auto* nonInheritedParentValue = state().parentStyle().nonInheritedCustomProperties().get(name)) {
            applyValue(*nonInheritedParentValue);
            return;
        }
        applyInitial();
    };

    return WTF::switchOn(WTFMove(parsedCustomProperty),
        [&](CSSWideKeyword&& keyword) {
            ApplyValueType valueType = ApplyValueType::Value;
            bool isRevert = false;
            bool isRevertLayer = false;

            auto isInheritedProperty = [&] {
                return registeredCustomProperty ? registeredCustomProperty->inherits : true;
            };

            auto unsetValueType = [&] {
                // https://drafts.csswg.org/css-cascade-4/#inherit-initial
                // The unset CSS-wide keyword acts as either inherit or initial, depending on whether the property is inherited or not.
                return isInheritedProperty() ? ApplyValueType::Inherit : ApplyValueType::Initial;
            };

            switch (keyword) {
            case CSSWideKeyword::Initial:
                valueType = ApplyValueType::Initial;
                break;
            case CSSWideKeyword::Inherit:
                valueType = ApplyValueType::Inherit;
                break;
            case CSSWideKeyword::Unset:
                valueType = unsetValueType();
                break;
            case CSSWideKeyword::Revert:
                isRevert = true;
                valueType = unsetValueType();
                break;
            case CSSWideKeyword::RevertLayer:
                isRevertLayer = true;
                valueType = unsetValueType();
                break;
            }

            if (isRevert || isRevertLayer) {
                // In @keyframes, 'revert-layer' rolls back the cascaded value to the author level.
                // We can just not apply the property in order to keep the value from the base style.
                if (isRevertLayer && m_state->m_isBuildingKeyframeStyle)
                    return;

                auto* rollbackCascade = isRevert ? ensureRollbackCascadeForRevert() : ensureRollbackCascadeForRevertLayer();

                if (rollbackCascade) {
                    // With the rollback cascade built, we need to obtain the property and apply it. If the property is
                    // not present, then we behave like "unset." Otherwise we apply the property instead of our own.
                    if (applyRollbackCascadeCustomProperty(*rollbackCascade, name))
                        return;
                }
            }

            if (!m_state->applyPropertyToRegularStyle()) {
                // Limit the properties that can be applied to only the ones honored by :visited.
                return;
            }

            if (valueType == ApplyValueType::Inherit && !isInheritedProperty())
                style.setHasExplicitlyInheritedProperties();

            switch (valueType) {
            case ApplyValueType::Initial:
                applyInitial();
                break;
            case ApplyValueType::Inherit:
                applyInherit();
                break;
            case ApplyValueType::Value:
                ASSERT_NOT_REACHED();
                break;
            };
        },
        [&](Ref<const CustomProperty>&& resolved) {
            if (!m_state->applyPropertyToRegularStyle()) {
                // Limit the properties that can be applied to only the ones honored by :visited.
                return;
            }
            applyValue(WTFMove(resolved));
        }
    );
}

Ref<CSSValue> Builder::resolveInternalAutoBaseFunction(CSSValue& value)
{
    RefPtr functionValue = dynamicDowncast<CSSFunctionValue>(value);
    if (!functionValue)
        return value;

    if (functionValue->name() != CSSValueInternalAutoBase)
        return value;

    RefPtr result = const_cast<CSSValue*>(m_state->style().appearance() == StyleAppearance::Base ? functionValue->item(1) : functionValue->item(0));

    if (!result)
        return value;

    return result.releaseNonNull();
}

Ref<CSSValue> Builder::resolveVariableReferences(CSSPropertyID propertyID, CSSValue& value)
{
    if (!value.hasVariableReferences())
        return value;

    auto variableValue = [&]() -> RefPtr<CSSValue> {
        if (auto* substitution = dynamicDowncast<CSSPendingSubstitutionValue>(value))
            return substitution->resolveValue(*this, propertyID);

        auto& variableReferenceValue = downcast<CSSVariableReferenceValue>(value);
        return variableReferenceValue.resolveSingleValue(*this, propertyID);
    }();

    // https://drafts.csswg.org/css-variables-2/#invalid-variables
    // ...as if the property’s value had been specified as the unset keyword.
    if (!variableValue || m_state->m_invalidAtComputedValueTimeProperties.get(propertyID))
        return CSSPrimitiveValue::create(CSSValueUnset);

    return *variableValue;
}

RefPtr<const CustomProperty> Builder::resolveCustomPropertyForContainerQueries(const CSSCustomPropertyValue& value)
{
    auto resolvedValue = resolveCustomPropertyValue(const_cast<CSSCustomPropertyValue&>(value));
    if (!resolvedValue)
        return CustomProperty::createForGuaranteedInvalid(value.name());

    return WTF::switchOn(*resolvedValue,
        [&](const CSSWideKeyword& keyword) -> RefPtr<const CustomProperty> {
            auto name = value.name();
            auto* registered = m_state->document().customPropertyRegistry().get(name);
            bool isInherited = !registered || registered->inherits;

            auto initial = [&]() -> RefPtr<const CustomProperty> {
                if (registered)
                    return registered->initialValue;
                return CustomProperty::createForGuaranteedInvalid(name);
            };

            auto inherit = [&]() -> RefPtr<const CustomProperty> {
                auto parentValue = isInherited
                    ? m_state->parentStyle().inheritedCustomProperties().get(name)
                    : m_state->parentStyle().nonInheritedCustomProperties().get(name);
                if (parentValue)
                    return parentValue;

                return initial();
            };

            switch (keyword) {
            case CSSWideKeyword::Initial:
                return initial();
            case CSSWideKeyword::Inherit:
                return inherit();
            case CSSWideKeyword::Unset:
                return isInherited ? inherit() : initial();
            case CSSWideKeyword::Revert:
            case CSSWideKeyword::RevertLayer:
                // https://drafts.csswg.org/css-contain-3/#style-container
                // "Cascade-dependent keywords, such as revert and revert-layer, are invalid as values in a style feature,
                // and cause the container style query to be false."
                return nullptr;
            }
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        },
        [](const Ref<const CustomProperty>& resolvedValue) -> RefPtr<const CustomProperty> {
            return resolvedValue.copyRef();
        }
    );
}

std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> Builder::resolveCustomPropertyValue(CSSCustomPropertyValue& value)
{
    auto name = value.name();

    if (auto keyword = value.tryCSSWideKeyword())
        return { { *keyword } };

    auto* registered = m_state->document().customPropertyRegistry().get(name);

    auto preResolved = switchOn(value.value(),
        [&](const Ref<CSSVariableReferenceValue>&) -> std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> {
            return { };
        },
        [&](const Ref<CSSVariableData>& data) -> std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> {
            if (!registered)
                return { { CustomProperty::createForVariableData(name, data.copyRef()) } };
            return { };
        },
        [&](const CSSWideKeyword& keyword) -> std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> {
            if (!registered)
                return { { keyword } };
            return { };
        }
    );
    if (preResolved)
        return preResolved;

    auto resolvedData = switchOn(value.value(),
        [&](const Ref<CSSVariableReferenceValue>& variableReferenceValue) -> RefPtr<CSSVariableData> {
            return variableReferenceValue->resolveVariableReferences(*this);
        },
        [&](const Ref<CSSVariableData>& data) -> RefPtr<CSSVariableData> {
            return data.ptr();
        },
        [&](const CSSWideKeyword&) -> RefPtr<CSSVariableData> {
            return nullptr;
        }
    );

    if (!resolvedData)
        return { };

    if (!registered) {
        // CSS-wide keywords are allowed in var() fallbacks of unregistered properties.
        if (auto keyword = CSSPropertyParser::parseCSSWideKeyword(resolvedData->tokens()))
            return { { *keyword } };

        return { { CustomProperty::createForVariableData(name, *resolvedData) } };
    }

    auto dependencies = CSSPropertyParser::collectParsedCustomPropertyValueDependencies(registered->syntax, resolvedData->tokens(), resolvedData->context());

    // https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
    bool hasCycles = false;
    bool isFontDependent = false;

    auto checkDependencies = [&](auto& propertyDependencies) {
        for (auto property : propertyDependencies) {
            if (m_state->m_inProgressProperties.get(property)) {
                m_state->m_invalidAtComputedValueTimeProperties.set(property);
                hasCycles = true;
            }
            if (property == CSSPropertyFontSize)
                isFontDependent = true;
        }
    };

    checkDependencies(dependencies.properties);

    if (m_state->element() == m_state->document().documentElement())
        checkDependencies(dependencies.rootProperties);

    if (hasCycles)
        return { };

    if (isFontDependent)
        m_state->updateFont();

    return CSSPropertyParser::parseTypedCustomPropertyValue(name, registered->syntax, resolvedData->tokens(), m_state, resolvedData->context());
}

void Builder::applyPageSizeDescriptor(CSSValue& value)
{
    m_state->style().resetPageSize();

    auto convertedPageSize = toStyleFromCSSValue<PageSize>(m_state, value);

    if (m_state->isCurrentPropertyInvalidAtComputedValueTime())
        return;

    m_state->style().setPageSize(WTFMove(convertedPageSize));
}

const PropertyCascade* Builder::ensureRollbackCascadeForRevert()
{
    auto rollbackOrigin = m_state->m_currentProperty->origin;

    switch (rollbackOrigin) {
    case PropertyCascade::Origin::PositionFallback:
        // "Similar to the Animation Origin, use of the revert value acts as if the property was part of the Author Origin."
        // https://drafts.csswg.org/css-anchor-position-1/#fallback-rule
    case PropertyCascade::Origin::Author:
        rollbackOrigin = PropertyCascade::Origin::User;
        break;
    case PropertyCascade::Origin::User:
        rollbackOrigin = PropertyCascade::Origin::UserAgent;
        break;
    case PropertyCascade::Origin::UserAgent:
        return nullptr;
    }

    auto key = makeRollbackCascadeKey(rollbackOrigin);
    return m_rollbackCascades.ensure(key, [&] {
        return makeUnique<const PropertyCascade>(m_cascade, rollbackOrigin);
    }).iterator->value.get();
}

const PropertyCascade* Builder::ensureRollbackCascadeForRevertLayer()
{
    auto& property = *m_state->m_currentProperty;
    auto rollbackLayerPriority = property.cascadeLayerPriority;
    if (!rollbackLayerPriority)
        return ensureRollbackCascadeForRevert();

    ASSERT(property.fromStyleAttribute == FromStyleAttribute::No || property.cascadeLayerPriority == RuleSet::cascadeLayerPriorityForUnlayered);

    // Style attribute reverts to the regular author style.
    if (property.fromStyleAttribute == FromStyleAttribute::No)
        --rollbackLayerPriority;

    auto key = makeRollbackCascadeKey(property.origin, property.styleScopeOrdinal, rollbackLayerPriority);
    return m_rollbackCascades.ensure(key, [&] {
        return makeUnique<const PropertyCascade>(m_cascade, property.origin, property.styleScopeOrdinal, rollbackLayerPriority);
    }).iterator->value.get();
}

auto Builder::makeRollbackCascadeKey(PropertyCascade::Origin cascadeOrigin, ScopeOrdinal scopeOrdinal, CascadeLayerPriority cascadeLayerPriority) -> RollbackCascadeKey
{
    static constexpr auto isNonEmptyValue = true;
    return { static_cast<unsigned>(cascadeOrigin), static_cast<unsigned>(scopeOrdinal), static_cast<unsigned>(cascadeLayerPriority), isNonEmptyValue };
}

}
}
