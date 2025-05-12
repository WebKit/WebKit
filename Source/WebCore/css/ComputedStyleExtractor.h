/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#pragma once

#include "PseudoElementIdentifier.h"
#include <span>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSFunctionValue;
class CSSPrimitiveValue;
class CSSValue;
class CSSValueList;
class CSSValuePool;
class Element;
class MutableStyleProperties;
class Node;
class RenderElement;
class RenderStyle;
class StylePropertyShorthand;
class TransformOperation;
class TransformationMatrix;

struct Length;
struct PropertyValue;

enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;

using CSSValueListBuilder = Vector<Ref<CSSValue>, 4>;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ComputedStyleExtractor);
class ComputedStyleExtractor {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ComputedStyleExtractor);
public:
    ComputedStyleExtractor(Node*, bool allowVisitedStyle = false);
    ComputedStyleExtractor(Node*, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>&);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle = false);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>&);

    enum class UpdateLayout : bool { No, Yes };
    enum class PropertyValueType : bool { Resolved, Computed };

    bool hasProperty(CSSPropertyID) const;
    RefPtr<CSSValue> propertyValue(CSSPropertyID, UpdateLayout = UpdateLayout::Yes, PropertyValueType = PropertyValueType::Resolved) const;
    RefPtr<CSSValue> valueForPropertyInStyle(const RenderStyle&, CSSPropertyID, CSSValuePool&, RenderElement* = nullptr, PropertyValueType = PropertyValueType::Resolved) const;
    String customPropertyText(const AtomString& propertyName) const;
    RefPtr<CSSValue> customPropertyValue(const AtomString& propertyName) const;

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyProperties(std::span<const CSSPropertyID>) const;
    Ref<MutableStyleProperties> copyProperties() const;
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
    bool propertyMatches(CSSPropertyID, const CSSValue*) const;
    bool propertyMatches(CSSPropertyID, CSSValueID) const;

    static Ref<CSSPrimitiveValue> valueForZoomAdjustedPixelLength(const RenderStyle&, const Length&);
    static Ref<CSSFunctionValue> valueForTransformationMatrix(const RenderStyle&, const TransformationMatrix&);
    static RefPtr<CSSFunctionValue> valueForTransformOperation(const RenderStyle&, const TransformOperation&);

    static bool updateStyleIfNeededForProperty(Element&, CSSPropertyID);

private:
    // The renderer we should use for resolving layout-dependent properties.
    RenderElement* styledRenderer() const;

    RefPtr<CSSValueList> valueForShorthandProperties(const StylePropertyShorthand&) const;
    RefPtr<CSSValueList> valueFor2SidesShorthand(const StylePropertyShorthand&) const;
    RefPtr<CSSValueList> valueFor4SidesShorthand(const StylePropertyShorthand&) const;

    RefPtr<CSSValue> valueForGridShorthand(const StylePropertyShorthand&) const;
    RefPtr<CSSValue> valueForTextWrapShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForWhiteSpaceShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForTextBoxShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForLineClampShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForContainerShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForColumnsShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForFlexFlowShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForPositionTryShorthand(const RenderStyle&) const;
    RefPtr<CSSValue> valueForFontVariantShorthand() const;
    RefPtr<CSSValue> valueForBackgroundShorthand() const;
    RefPtr<CSSValue> valueForMaskShorthand() const;
    RefPtr<CSSValue> valueForBorderShorthand() const;
    RefPtr<CSSValue> valueForBorderBlockShorthand() const;
    RefPtr<CSSValue> valueForBorderInlineShorthand() const;

    size_t layerCount(CSSPropertyID) const;
    Ref<CSSValue> fillLayerPropertyShorthandValue(CSSPropertyID, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty) const;

    RefPtr<Element> m_element;
    std::optional<Style::PseudoElementIdentifier> m_pseudoElementIdentifier;
    bool m_allowVisitedStyle;
};

} // namespace WebCore
