/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#include <span>
#include <wtf/RefPtr.h>

namespace WebCore {

class Animation;
class CSSFunctionValue;
class CSSPrimitiveValue;
class CSSValue;
class CSSValueList;
class Element;
class FilterOperations;
class MutableStyleProperties;
class Node;
class RenderElement;
class RenderStyle;
class ShadowData;
class StyleColor;
class StylePropertyShorthand;
class TransformOperation;

struct PropertyValue;

enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;

enum class PseudoId : uint32_t;
enum class SVGPaintType : uint8_t;

using CSSValueListBuilder = Vector<Ref<CSSValue>, 4>;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ComputedStyleExtractor);
class ComputedStyleExtractor {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ComputedStyleExtractor);
public:
    ComputedStyleExtractor(Node*, bool allowVisitedStyle = false);
    ComputedStyleExtractor(Node*, bool allowVisitedStyle, PseudoId);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle = false);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle, PseudoId);

    enum class UpdateLayout : bool { No, Yes };
    enum class PropertyValueType : bool { Resolved, Computed };
    bool hasProperty(CSSPropertyID) const;
    RefPtr<CSSValue> propertyValue(CSSPropertyID, UpdateLayout = UpdateLayout::Yes, PropertyValueType = PropertyValueType::Resolved) const;
    RefPtr<CSSValue> valueForPropertyInStyle(const RenderStyle&, CSSPropertyID, RenderElement* = nullptr, PropertyValueType = PropertyValueType::Resolved) const;
    String customPropertyText(const AtomString& propertyName) const;
    RefPtr<CSSValue> customPropertyValue(const AtomString& propertyName) const;

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyProperties(std::span<const CSSPropertyID>) const;
    Ref<MutableStyleProperties> copyProperties() const;
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
    bool propertyMatches(CSSPropertyID, const CSSValue*) const;
    bool propertyMatches(CSSPropertyID, CSSValueID) const;

    enum class AdjustPixelValuesForComputedStyle : bool { No, Yes };
    static Ref<CSSValue> valueForFilter(const RenderStyle&, const FilterOperations&, AdjustPixelValuesForComputedStyle = AdjustPixelValuesForComputedStyle::Yes);

    static Ref<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle&, const StyleColor&);

    static bool updateStyleIfNeededForProperty(Element&, CSSPropertyID);

private:
    // The renderer we should use for resolving layout-dependent properties.
    RenderElement* styledRenderer() const;

    RefPtr<CSSValue> svgPropertyValue(CSSPropertyID) const;
    Ref<CSSValue> adjustSVGPaint(SVGPaintType, const String& url, Ref<CSSPrimitiveValue> color) const;
    static Ref<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle&, AdjustPixelValuesForComputedStyle = AdjustPixelValuesForComputedStyle::Yes);

    Ref<CSSValueList> getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand&) const;
    RefPtr<CSSValueList> getCSSPropertyValuesFor2SidesShorthand(const StylePropertyShorthand&) const;
    RefPtr<CSSValueList> getCSSPropertyValuesFor4SidesShorthand(const StylePropertyShorthand&) const;

    size_t getLayerCount(CSSPropertyID) const;
    Ref<CSSValue> getFillLayerPropertyShorthandValue(CSSPropertyID, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty) const;
    Ref<CSSValue> getBackgroundShorthandValue() const;
    Ref<CSSValue> getMaskShorthandValue() const;
    Ref<CSSValueList> getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand&) const;
    Ref<CSSValue> fontVariantShorthandValue() const;
    RefPtr<CSSValue> textWrapShorthandValue(const RenderStyle&) const;
    RefPtr<CSSValue> whiteSpaceShorthandValue(const RenderStyle&) const;

    RefPtr<Element> m_element;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
};

RefPtr<CSSFunctionValue> transformOperationAsCSSValue(const TransformOperation&, const RenderStyle&);

} // namespace WebCore
