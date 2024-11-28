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
#include "ComputedStyleDependencies.h"
#include "CustomPropertyRegistry.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "HTMLElement.h"
#include "PaintWorkletGlobalScope.h"
#include "RenderStyleSetters.h"
#include "Settings.h"
#include "StyleBuilderGenerated.h"
#include "StyleCustomPropertyData.h"
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

Builder::Builder(RenderStyle& style, BuilderContext&& context, const MatchResult& matchResult, CascadeLevel cascadeLevel, OptionSet<PropertyCascade::PropertyType> includedProperties, const HashSet<AnimatableCSSProperty>* animatedPropertes)
    : m_cascade(matchResult, cascadeLevel, includedProperties, animatedPropertes)
    , m_state(*this, style, WTFMove(context))
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
    // This needs to apply before other properties for the `lh` unit, but after updating the font.
    applyProperties(CSSPropertyLineHeight, CSSPropertyLineHeight);
}

void Builder::applyNonHighPriorityProperties()
{
    ASSERT(!m_state.fontDirty());

    applyProperties(firstLowPriorityProperty, lastLowPriorityProperty);
    applyLogicalGroupProperties();
    // Any referenced custom properties are already resolved. This will resolve the remaining ones.
    applyCustomProperties();

    ASSERT(!m_state.fontDirty());
}

void Builder::applyLogicalGroupProperties()
{
    // Properties in a logical property group are applied in author specified order which is maintained separately for them.
    for (auto id : m_cascade.logicalGroupPropertyIDs())
        applyCascadeProperty(m_cascade.logicalGroupProperty(id));
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
    auto applyProperty = [&](size_t index) ALWAYS_INLINE_LAMBDA {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(index);
        ASSERT(propertyID != CSSPropertyCustom);
        auto& property = m_cascade.normalProperty(propertyID);

        if constexpr (trackCycles == CustomPropertyCycleTracking::Enabled) {
            m_state.m_inProgressProperties.set(propertyID);
            applyCascadeProperty(property);
            m_state.m_inProgressProperties.clear(propertyID);
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
        if (m_state.m_appliedCustomProperties.contains(name))
            continue;
        applyCustomPropertyImpl(name, value);
    }
}

void Builder::applyCustomProperty(const AtomString& name)
{
    if (m_state.m_appliedCustomProperties.contains(name))
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

    bool inCycle = !m_state.m_inProgressCustomProperties.add(name).isNewEntry;
    if (inCycle) {
        auto isNewCycle = m_state.m_inCycleCustomProperties.add(name).isNewEntry;
        if (isNewCycle) {
            // Continue resolving dependencies so we detect cycles for them as well.
            resolveCustomPropertyValue(customPropertyValue.get());
        }
        return;
    }
    
    // There may be multiple cycles through the same property. Avoid interference from any previously detected cycles.
    auto savedInCycleProperties = std::exchange(m_state.m_inCycleCustomProperties, { });

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

    auto resolvedValue = resolveCustomPropertyValue(customPropertyValue.get());

    if (!resolvedValue || m_state.m_inCycleCustomProperties.contains(name))
        resolvedValue = createInvalidOrUnset();

    SetForScope levelScope(m_state.m_currentProperty, &property);
    SetForScope scopedLinkMatchMutation(m_state.m_linkMatch, SelectorChecker::MatchDefault);
    applyProperty(CSSPropertyCustom, *resolvedValue, SelectorChecker::MatchDefault);

    m_state.m_inProgressCustomProperties.remove(name);
    m_state.m_appliedCustomProperties.add(name);
    m_state.m_inCycleCustomProperties.formUnion(WTFMove(savedInCycleProperties));
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
    ASSERT_WITH_MESSAGE(!isShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    auto valueToApply = resolveVariableReferences(id, value);
    auto& style = m_state.style();

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, style.writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), linkMatchMask);
    }

    auto valueID = WebCore::valueID(valueToApply.get());

    const CSSCustomPropertyValue* customPropertyValue = nullptr;
    const CSSRegisteredCustomProperty* registeredCustomProperty = nullptr;

    if (id == CSSPropertyCustom) {
        customPropertyValue = downcast<CSSCustomPropertyValue>(valueToApply.ptr());
        ASSERT(customPropertyValue->isResolved());
        if (std::holds_alternative<CSSValueID>(customPropertyValue->value()))
            valueID = std::get<CSSValueID>(customPropertyValue->value());
        auto& name = customPropertyValue->name();
        registeredCustomProperty = m_state.document().customPropertyRegistry().get(name);
    }

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
                if (registeredCustomProperty && registeredCustomProperty->inherits) {
                    auto iterator = rollbackCascade->customProperties().find(customPropertyValue->name());
                    if (iterator != rollbackCascade->customProperties().end()) {
                        applyRollbackCascadeProperty(iterator->value, linkMatchMask);
                        return;
                    }
                }
            } else if (id < firstLogicalGroupProperty) {
                if (rollbackCascade->hasNormalProperty(id)) {
                    auto& property = rollbackCascade->normalProperty(id);
                    applyRollbackCascadeProperty(property, linkMatchMask);
                    return;
                }
            } else if (auto* property = rollbackCascade->lastPropertyResolvingLogicalPropertyPair(id, style.writingMode())) {
                applyRollbackCascadeProperty(*property, linkMatchMask);
                return;
            }
        }

        isUnset = true;
    }

    auto isInheritedProperty = [&] {
        return registeredCustomProperty ? registeredCustomProperty->inherits : CSSProperty::isInheritedProperty(id);
    };

    auto unsetValueType = [&] {
        // https://drafts.csswg.org/css-cascade-4/#inherit-initial
        // The unset CSS-wide keyword acts as either inherit or initial, depending on whether the property is inherited or not.
        return isInheritedProperty() ? ApplyValueType::Inherit : ApplyValueType::Initial;
    };

    if (isUnset)
        valueType = unsetValueType();

    if (!m_state.applyPropertyToRegularStyle() && !isValidVisitedLinkProperty(id)) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (valueType == ApplyValueType::Inherit && !isInheritedProperty())
        style.setHasExplicitlyInheritedProperties();

    if (auto* paintImageValue = dynamicDowncast<CSSPaintImageValue>(valueToApply.get())) {
        auto& name = paintImageValue->name();
        if (auto* paintWorklet = const_cast<Document&>(m_state.document()).paintWorkletGlobalScopeForName(name)) {
            Locker locker { paintWorklet->paintDefinitionLock() };
            if (auto* registration = paintWorklet->paintDefinitionMap().get(name)) {
                for (auto& property : registration->inputProperties)
                    style.addCustomPaintWatchProperty(property);
            }
        }
    }

    if (customPropertyValue) {
        ASSERT(id == CSSPropertyCustom);
        applyCustomPropertyValue(*customPropertyValue, valueType, registeredCustomProperty);
        return;
    }

    if (UNLIKELY(id == CSSPropertySize && valueType == ApplyValueType::Value)) {
        applyPageSizeDescriptor(valueToApply.get());
        return;
    }

    BuilderGenerated::applyProperty(id, m_state, valueToApply.get(), valueType);

    if (!isUnset && m_state.isCurrentPropertyInvalidAtComputedValueTime()) {
        // https://drafts.csswg.org/css-variables-2/#invalid-variables
        // A declaration can be invalid at computed-value time if...
        // When this happens, the computed value is one of the following...
        // Otherwise: Either the property’s inherited value or its initial value depending on whether the property
        // is inherited or not, respectively, as if the property’s value had been specified as the unset keyword
        BuilderGenerated::applyProperty(id, m_state, valueToApply.get(), unsetValueType());
    }
}

void Builder::applyCustomPropertyValue(const CSSCustomPropertyValue& value, ApplyValueType valueType, const CSSRegisteredCustomProperty* registered)
{
    auto applyValue = [&](auto& valueToApply) {
        ASSERT(valueToApply.isResolved());

        bool isInherited = !registered || registered->inherits;
        state().style().setCustomPropertyValue(valueToApply, isInherited);
    };

    auto applyInitial = [&] {
        if (registered && registered->initialValue) {
            applyValue(*registered->initialValue);
            return;
        }
        auto invalid = CSSCustomPropertyValue::createWithID(value.name(), CSSValueInvalid);
        applyValue(invalid.get());
    };

    auto applyInherit = [&] {
        auto* parentValue = state().parentStyle().inheritedCustomProperties().get(value.name());
        if (parentValue && !(registered && !registered->inherits)) {
            applyValue(const_cast<CSSCustomPropertyValue&>(*parentValue));
            return;
        }
        if (auto* nonInheritedParentValue = state().parentStyle().nonInheritedCustomProperties().get(value.name())) {
            applyValue(const_cast<CSSCustomPropertyValue&>(*nonInheritedParentValue));
            return;
        }
        applyInitial();
    };

    switch (valueType) {
    case ApplyValueType::Initial:
        applyInitial();
        break;
    case ApplyValueType::Inherit:
        applyInherit();
        break;
    case ApplyValueType::Value:
        applyValue(value);
        break;
    };
}

Ref<CSSValue> Builder::resolveVariableReferences(CSSPropertyID propertyID, CSSValue& value)
{
    if (!value.hasVariableReferences())
        return value;

    auto variableValue = [&]() -> RefPtr<CSSValue> {
        if (auto* substitution = dynamicDowncast<CSSPendingSubstitutionValue>(value))
            return substitution->resolveValue(m_state, propertyID);

        auto& variableReferenceValue = downcast<CSSVariableReferenceValue>(value);
        return variableReferenceValue.resolveSingleValue(m_state, propertyID);
    }();

    // https://drafts.csswg.org/css-variables-2/#invalid-variables
    // ...as if the property’s value had been specified as the unset keyword.
    if (!variableValue || m_state.m_invalidAtComputedValueTimeProperties.get(propertyID))
        return CSSPrimitiveValue::create(CSSValueUnset);

    return *variableValue;
}

RefPtr<const CSSCustomPropertyValue> Builder::resolveCustomPropertyForContainerQueries(const CSSCustomPropertyValue& value)
{
    if (value.containsCSSWideKeyword()) {
        auto name = value.name();
        auto* registered = m_state.document().customPropertyRegistry().get(name);
        bool isInherited = !registered || registered->inherits;

        auto initial = [&]() -> RefPtr<const CSSCustomPropertyValue> {
            if (registered)
                return registered->initialValue;
            return CSSCustomPropertyValue::createWithID(name, CSSValueInvalid);
        };

        auto inherit = [&]() -> RefPtr<const CSSCustomPropertyValue> {
            auto parentValue = isInherited
                ? m_state.parentStyle().inheritedCustomProperties().get(name)
                : m_state.parentStyle().nonInheritedCustomProperties().get(name);
            if (parentValue)
                return parentValue;

            return initial();
        };

        auto valueId = std::get<CSSValueID>(value.value());
        switch (valueId) {
        case CSSValueInitial:
            return initial();
        case CSSValueInherit:
            return inherit();
        case CSSValueUnset:
            return isInherited ? inherit() : initial();
        case CSSValueRevert:
        case CSSValueRevertLayer:
            // https://drafts.csswg.org/css-contain-3/#style-container
            // "Cascade-dependent keywords, such as revert and revert-layer, are invalid as values in a style feature,
            // and cause the container style query to be false."
            return nullptr;
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto resolvedValue = resolveCustomPropertyValue(const_cast<CSSCustomPropertyValue&>(value));
    if (!resolvedValue)
        return CSSCustomPropertyValue::createWithID(value.name(), CSSValueInvalid);

    return resolvedValue;
}

RefPtr<CSSCustomPropertyValue> Builder::resolveCustomPropertyValue(CSSCustomPropertyValue& value)
{
    if (value.containsCSSWideKeyword())
        return &value;

    auto name = value.name();
    auto* registered = m_state.document().customPropertyRegistry().get(name);

    if (value.isResolved() && !registered)
        return &value;

    auto resolvedData = switchOn(value.value(), [&](const Ref<CSSVariableReferenceValue>& variableReferenceValue) {
        return variableReferenceValue->resolveVariableReferences(m_state);
    }, [&](const Ref<CSSVariableData>& data) -> RefPtr<CSSVariableData> {
        return data.ptr();
    }, [&](auto&) -> RefPtr<CSSVariableData> {
        return nullptr;
    });

    if (!resolvedData)
        return nullptr;

    if (!registered)
        return CSSCustomPropertyValue::createSyntaxAll(name, *resolvedData);

    auto dependencies = CSSPropertyParser::collectParsedCustomPropertyValueDependencies(registered->syntax, resolvedData->tokens(), resolvedData->context());

    // https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
    bool hasCycles = false;
    bool isFontDependent = false;

    auto checkDependencies = [&](auto& propertyDependencies) {
        for (auto property : propertyDependencies) {
            if (m_state.m_inProgressProperties.get(property)) {
                m_state.m_invalidAtComputedValueTimeProperties.set(property);
                hasCycles = true;
            }
            if (property == CSSPropertyFontSize)
                isFontDependent = true;
        }
    };

    checkDependencies(dependencies.properties);

    if (m_state.element() == m_state.document().documentElement())
        checkDependencies(dependencies.rootProperties);

    if (hasCycles)
        return nullptr;

    if (isFontDependent)
        m_state.updateFont();

    return CSSPropertyParser::parseTypedCustomPropertyValue(name, registered->syntax, resolvedData->tokens(), m_state, resolvedData->context());
}

static bool pageSizeFromName(const CSSPrimitiveValue& pageSizeName, const CSSPrimitiveValue* pageOrientation, WebCore::Length& width, WebCore::Length& height)
{
    auto mmLength = [](double mm) {
        return WebCore::Length(CSS::pixelsPerMm * mm, LengthType::Fixed);
    };

    auto inchLength = [](double inch) {
        return WebCore::Length(CSS::pixelsPerInch * inch, LengthType::Fixed);
    };

    static NeverDestroyed<WebCore::Length> a5Width(mmLength(148));
    static NeverDestroyed<WebCore::Length> a5Height(mmLength(210));
    static NeverDestroyed<WebCore::Length> a4Width(mmLength(210));
    static NeverDestroyed<WebCore::Length> a4Height(mmLength(297));
    static NeverDestroyed<WebCore::Length> a3Width(mmLength(297));
    static NeverDestroyed<WebCore::Length> a3Height(mmLength(420));
    static NeverDestroyed<WebCore::Length> b5Width(mmLength(176));
    static NeverDestroyed<WebCore::Length> b5Height(mmLength(250));
    static NeverDestroyed<WebCore::Length> b4Width(mmLength(250));
    static NeverDestroyed<WebCore::Length> b4Height(mmLength(353));
    static NeverDestroyed<WebCore::Length> jisB5Width(mmLength(182));
    static NeverDestroyed<WebCore::Length> jisB5Height(mmLength(257));
    static NeverDestroyed<WebCore::Length> jisB4Width(mmLength(257));
    static NeverDestroyed<WebCore::Length> jisB4Height(mmLength(364));
    static NeverDestroyed<WebCore::Length> letterWidth(inchLength(8.5));
    static NeverDestroyed<WebCore::Length> letterHeight(inchLength(11));
    static NeverDestroyed<WebCore::Length> legalWidth(inchLength(8.5));
    static NeverDestroyed<WebCore::Length> legalHeight(inchLength(14));
    static NeverDestroyed<WebCore::Length> ledgerWidth(inchLength(11));
    static NeverDestroyed<WebCore::Length> ledgerHeight(inchLength(17));

    switch (pageSizeName.valueID()) {
    case CSSValueA5:
        width = a5Width;
        height = a5Height;
        break;
    case CSSValueA4:
        width = a4Width;
        height = a4Height;
        break;
    case CSSValueA3:
        width = a3Width;
        height = a3Height;
        break;
    case CSSValueB5:
        width = b5Width;
        height = b5Height;
        break;
    case CSSValueB4:
        width = b4Width;
        height = b4Height;
        break;
    case CSSValueJisB5:
        width = jisB5Width;
        height = jisB5Height;
        break;
    case CSSValueJisB4:
        width = jisB4Width;
        height = jisB4Height;
        break;
    case CSSValueLetter:
        width = letterWidth;
        height = letterHeight;
        break;
    case CSSValueLegal:
        width = legalWidth;
        height = legalHeight;
        break;
    case CSSValueLedger:
        width = ledgerWidth;
        height = ledgerHeight;
        break;
    default:
        return false;
    }

    if (pageOrientation) {
        switch (pageOrientation->valueID()) {
        case CSSValueLandscape:
            std::swap(width, height);
            break;
        case CSSValuePortrait:
            // Nothing to do.
            break;
        default:
            return false;
        }
    }
    return true;
}

void Builder::applyPageSizeDescriptor(CSSValue& value)
{
    m_state.style().resetPageSizeType();

    WebCore::Length width;
    WebCore::Length height;
    auto pageSizeType = PageSizeType::Auto;

    if (auto* pair = dynamicDowncast<CSSValuePair>(value)) {
        // <length>{2} | <page-size> <orientation>
        auto* first = dynamicDowncast<CSSPrimitiveValue>(pair->first());
        auto* second = dynamicDowncast<CSSPrimitiveValue>(pair->second());
        if (!first || !second)
            return;
        if (first->isLength()) {
            // <length>{2}
            if (!second->isLength())
                return;
            auto conversionData = m_state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f);
            width = first->resolveAsLength<WebCore::Length>(conversionData);
            height = second->resolveAsLength<WebCore::Length>(conversionData);
        } else {
            // <page-size> <orientation>
            // The value order is guaranteed. See CSSParser::parseSizeParameter.
            if (!pageSizeFromName(*first, second, width, height))
                return;
        }
        pageSizeType = PageSizeType::Resolved;
    } else if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        // <length> | auto | <page-size> | [ portrait | landscape]
        if (primitiveValue->isLength()) {
            // <length>
            pageSizeType = PageSizeType::Resolved;
            width = height = primitiveValue->resolveAsLength<WebCore::Length>(m_state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        } else {
            switch (primitiveValue->valueID()) {
            case CSSValueInvalid:
                return;
            case CSSValueAuto:
                pageSizeType = PageSizeType::Auto;
                break;
            case CSSValuePortrait:
                pageSizeType = PageSizeType::AutoPortrait;
                break;
            case CSSValueLandscape:
                pageSizeType = PageSizeType::AutoLandscape;
                break;
            default:
                // <page-size>
                pageSizeType = PageSizeType::Resolved;
                if (!pageSizeFromName(*primitiveValue, nullptr, width, height))
                    return;
            }
        }
    } else
        return;

    m_state.style().setPageSizeType(pageSizeType);
    m_state.style().setPageSize({ WTFMove(width), WTFMove(height) });
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
