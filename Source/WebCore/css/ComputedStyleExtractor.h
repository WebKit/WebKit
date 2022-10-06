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

#include "RenderStyleConstants.h"
#include "SVGRenderStyleDefs.h"
#include "TextFlags.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Animation;
class CSSBorderImageSliceValue;
class CSSFontStyleValue;
class CSSPrimitiveValue;
class CSSValueList;
class Color;
class Element;
class FilterOperations;
class FontSelectionValue;
class MutableStyleProperties;
class Node;
class RenderElement;
class RenderStyle;
class SVGPaint;
class ShadowData;
class StylePropertyShorthand;

class ComputedStyleExtractor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ComputedStyleExtractor(Node*, bool allowVisitedStyle = false, PseudoId = PseudoId::None);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle = false, PseudoId = PseudoId::None);

    enum class UpdateLayout : uint8_t { Yes, No };
    enum class PropertyValueType : uint8_t { Resolved, Computed };
    RefPtr<CSSValue> propertyValue(CSSPropertyID, UpdateLayout = UpdateLayout::Yes, PropertyValueType = PropertyValueType::Resolved);
    RefPtr<CSSValue> valueForPropertyInStyle(const RenderStyle&, CSSPropertyID, RenderElement* = nullptr, PropertyValueType = PropertyValueType::Resolved);
    String customPropertyText(const AtomString& propertyName);
    RefPtr<CSSValue> customPropertyValue(const AtomString& propertyName);

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length);
    Ref<MutableStyleProperties> copyProperties();
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword();
    bool useFixedFontDefaultSize();
    bool propertyMatches(CSSPropertyID, const CSSValue*);

    enum class AdjustPixelValuesForComputedStyle : uint8_t { Yes, No };
    static Ref<CSSPrimitiveValue> zoomAdjustedPixelValue(double, const RenderStyle&);
    static Ref<CSSPrimitiveValue> zoomAdjustedPixelValueForLength(const Length&, const RenderStyle&);

    static Ref<CSSValue> valueForFilter(const RenderStyle&, const FilterOperations&, AdjustPixelValuesForComputedStyle = AdjustPixelValuesForComputedStyle::Yes);

    static Ref<CSSPrimitiveValue> fontNonKeywordWeightFromStyleValue(FontSelectionValue);
    static Ref<CSSPrimitiveValue> fontWeightFromStyleValue(FontSelectionValue);
    static Ref<CSSPrimitiveValue> fontNonKeywordStretchFromStyleValue(FontSelectionValue);
    static Ref<CSSPrimitiveValue> fontStretchFromStyleValue(FontSelectionValue);
    static Ref<CSSFontStyleValue> fontNonKeywordStyleFromStyleValue(FontSelectionValue);
    static Ref<CSSFontStyleValue> fontStyleFromStyleValue(std::optional<FontSelectionValue>, FontStyleAxis);

    static void addValueForAnimationPropertyToList(CSSValueList&, CSSPropertyID, const Animation*);

    static bool updateStyleIfNeededForProperty(Element&, CSSPropertyID);

private:
    // The renderer we should use for resolving layout-dependent properties.
    RenderElement* styledRenderer() const;

    RefPtr<CSSValue> svgPropertyValue(CSSPropertyID);
    Ref<CSSValue> adjustSVGPaint(SVGPaintType, const String& url, Ref<CSSPrimitiveValue> color) const;
    static Ref<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle&, AdjustPixelValuesForComputedStyle = AdjustPixelValuesForComputedStyle::Yes);
    Ref<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle&, const StyleColor&) const;

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

} // namespace WebCore
