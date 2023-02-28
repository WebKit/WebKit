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

#include <wtf/RefPtr.h>
#include <wtf/Span.h>

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

enum class PseudoId : uint16_t;
enum class SVGPaintType : uint8_t;

class ComputedStyleExtractor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ComputedStyleExtractor(Node*, bool allowVisitedStyle = false);
    ComputedStyleExtractor(Node*, bool allowVisitedStyle, PseudoId);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle = false);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle, PseudoId);

    enum class UpdateLayout : bool { No, Yes };
    enum class PropertyValueType : bool { Resolved, Computed };
    bool hasProperty(CSSPropertyID);
    RefPtr<CSSValue> propertyValue(CSSPropertyID, UpdateLayout = UpdateLayout::Yes, PropertyValueType = PropertyValueType::Resolved);
    RefPtr<CSSValue> valueForPropertyInStyle(const RenderStyle&, CSSPropertyID, RenderElement* = nullptr, PropertyValueType = PropertyValueType::Resolved);
    String customPropertyText(const AtomString& propertyName);
    RefPtr<CSSValue> customPropertyValue(const AtomString& propertyName);

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyProperties(Span<const CSSPropertyID>);
    Ref<MutableStyleProperties> copyProperties();
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword();
    bool useFixedFontDefaultSize();
    bool propertyMatches(CSSPropertyID, const CSSValue*);
    bool propertyMatches(CSSPropertyID, CSSValueID);

    enum class AdjustPixelValuesForComputedStyle : bool { No, Yes };
    static Ref<CSSValue> valueForFilter(const RenderStyle&, const FilterOperations&, AdjustPixelValuesForComputedStyle = AdjustPixelValuesForComputedStyle::Yes);

    static Ref<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle&, const StyleColor&);

    static void addValueForAnimationPropertyToList(CSSValueList&, CSSPropertyID, const Animation*);

    static bool updateStyleIfNeededForProperty(Element&, CSSPropertyID);

private:
    // The renderer we should use for resolving layout-dependent properties.
    RenderElement* styledRenderer() const;

    RefPtr<CSSValue> svgPropertyValue(CSSPropertyID);
    Ref<CSSValue> adjustSVGPaint(SVGPaintType, const String& url, Ref<CSSPrimitiveValue> color) const;
    static Ref<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle&, AdjustPixelValuesForComputedStyle = AdjustPixelValuesForComputedStyle::Yes);

    Ref<CSSValueList> getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand&);
    RefPtr<CSSValueList> getCSSPropertyValuesFor2SidesShorthand(const StylePropertyShorthand&);
    RefPtr<CSSValueList> getCSSPropertyValuesFor4SidesShorthand(const StylePropertyShorthand&);

    size_t getLayerCount(CSSPropertyID);
    Ref<CSSValue> getFillLayerPropertyShorthandValue(CSSPropertyID, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty);
    Ref<CSSValue> getBackgroundShorthandValue();
    Ref<CSSValue> getMaskShorthandValue();
    Ref<CSSValueList> getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand&);
    Ref<CSSValue> fontVariantShorthandValue();

    RefPtr<Element> m_element;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
};

RefPtr<CSSFunctionValue> transformOperationAsCSSValue(const TransformOperation&, const RenderStyle&);

} // namespace WebCore
