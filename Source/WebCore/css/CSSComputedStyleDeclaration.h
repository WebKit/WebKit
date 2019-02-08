/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#include "CSSStyleDeclaration.h"
#include "RenderStyleConstants.h"
#include "SVGRenderStyleDefs.h"
#include "TextFlags.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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
class StyleProperties;
class StylePropertyShorthand;

enum EUpdateLayout { DoNotUpdateLayout = false, UpdateLayout = true };

enum AdjustPixelValuesForComputedStyle { AdjustPixelValues, DoNotAdjustPixelValues };

class ComputedStyleExtractor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ComputedStyleExtractor(Node*, bool allowVisitedStyle = false, PseudoId = PseudoId::None);
    ComputedStyleExtractor(Element*, bool allowVisitedStyle = false, PseudoId = PseudoId::None);

    RefPtr<CSSValue> propertyValue(CSSPropertyID, EUpdateLayout = UpdateLayout);
    RefPtr<CSSValue> valueForPropertyinStyle(const RenderStyle&, CSSPropertyID, RenderElement* = nullptr);
    String customPropertyText(const String& propertyName);
    RefPtr<CSSValue> customPropertyValue(const String& propertyName);

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length);
    Ref<MutableStyleProperties> copyProperties();
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword();
    bool useFixedFontDefaultSize();
    bool propertyMatches(CSSPropertyID, const CSSValue*);

    static Ref<CSSValue> valueForFilter(const RenderStyle&, const FilterOperations&, AdjustPixelValuesForComputedStyle = AdjustPixelValues);

    static Ref<CSSPrimitiveValue> fontNonKeywordWeightFromStyleValue(FontSelectionValue);
    static Ref<CSSPrimitiveValue> fontWeightFromStyleValue(FontSelectionValue);
    static Ref<CSSPrimitiveValue> fontNonKeywordStretchFromStyleValue(FontSelectionValue);
    static Ref<CSSPrimitiveValue> fontStretchFromStyleValue(FontSelectionValue);
    static Ref<CSSFontStyleValue> fontNonKeywordStyleFromStyleValue(FontSelectionValue);
    static Ref<CSSFontStyleValue> fontStyleFromStyleValue(Optional<FontSelectionValue>, FontStyleAxis);

private:
    // The styled element is either the element passed into
    // computedPropertyValue, or the PseudoElement for :before and :after if
    // they exist.
    Element* styledElement() const;

    // The renderer we should use for resolving layout-dependent properties.
    // Note that it differs from styledElement()->renderer() in the case we have
    // no pseudo-element.
    RenderElement* styledRenderer() const;

    RefPtr<CSSValue> svgPropertyValue(CSSPropertyID, EUpdateLayout);
    Ref<CSSValue> adjustSVGPaintForCurrentColor(SVGPaintType, const String& url, const Color&, const Color& currentColor) const;
    static Ref<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle&, AdjustPixelValuesForComputedStyle = AdjustPixelValues);
    Ref<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle*, const Color&) const;

    Ref<CSSValueList> getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand&);
    RefPtr<CSSValueList> getCSSPropertyValuesFor2SidesShorthand(const StylePropertyShorthand&);
    RefPtr<CSSValueList> getCSSPropertyValuesFor4SidesShorthand(const StylePropertyShorthand&);
    Ref<CSSValueList> getBackgroundShorthandValue();
    Ref<CSSValueList> getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand&);

    RefPtr<Element> m_element;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
};

class CSSComputedStyleDeclaration final : public CSSStyleDeclaration {
public:
    static Ref<CSSComputedStyleDeclaration> create(Element& element, bool allowVisitedStyle = false, const String& pseudoElementName = String())
    {
        return adoptRef(*new CSSComputedStyleDeclaration(element, allowVisitedStyle, pseudoElementName));
    }
    virtual ~CSSComputedStyleDeclaration();

    WEBCORE_EXPORT void ref() final;
    WEBCORE_EXPORT void deref() final;

    String getPropertyValue(CSSPropertyID) const;

private:
    WEBCORE_EXPORT CSSComputedStyleDeclaration(Element&, bool allowVisitedStyle, const String&);

    // CSSOM functions. Don't make these public.
    CSSRule* parentRule() const final;
    unsigned length() const final;
    String item(unsigned index) const final;
    RefPtr<DeprecatedCSSOMValue> getPropertyCSSValue(const String& propertyName) final;
    String getPropertyValue(const String& propertyName) final;
    String getPropertyPriority(const String& propertyName) final;
    String getPropertyShorthand(const String& propertyName) final;
    bool isPropertyImplicit(const String& propertyName) final;
    ExceptionOr<void> setProperty(const String& propertyName, const String& value, const String& priority) final;
    ExceptionOr<String> removeProperty(const String& propertyName) final;
    String cssText() const final;
    ExceptionOr<void> setCssText(const String&) final;
    RefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) final;
    String getPropertyValueInternal(CSSPropertyID) final;
    ExceptionOr<bool> setPropertyInternal(CSSPropertyID, const String& value, bool important) final;
    Ref<MutableStyleProperties> copyProperties() const final;

    RefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID, EUpdateLayout = UpdateLayout) const;

    mutable Ref<Element> m_element;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
    unsigned m_refCount;
};

} // namespace WebCore
