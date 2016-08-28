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

#ifndef CSSComputedStyleDeclaration_h
#define CSSComputedStyleDeclaration_h

#include "CSSStyleDeclaration.h"
#include "RenderStyleConstants.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSValueList;
class Color;
class Element;
class FilterOperations;
class MutableStyleProperties;
class Node;
class RenderObject;
class RenderStyle;
class SVGPaint;
class ShadowData;
class StyleProperties;
class StylePropertyShorthand;

enum EUpdateLayout { DoNotUpdateLayout = false, UpdateLayout = true };

enum AdjustPixelValuesForComputedStyle { AdjustPixelValues, DoNotAdjustPixelValues };

class ComputedStyleExtractor {
public:
    ComputedStyleExtractor(RefPtr<Node>&&, bool allowVisitedStyle = false, PseudoId = NOPSEUDO);

    RefPtr<CSSValue> propertyValue(CSSPropertyID, EUpdateLayout = UpdateLayout) const;
    String customPropertyText(const String& propertyName) const;
    RefPtr<CSSValue> customPropertyValue(const String& propertyName) const;

    // Helper methods for HTML editing.
    Ref<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    Ref<MutableStyleProperties> copyProperties() const;
    RefPtr<CSSPrimitiveValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
    bool propertyMatches(CSSPropertyID, const CSSValue*) const;

    static Ref<CSSValue> valueForFilter(const RenderStyle&, const FilterOperations&, AdjustPixelValuesForComputedStyle = AdjustPixelValues);

private:
    // The styled node is either the node passed into computedPropertyValue, or the
    // PseudoElement for :before and :after if they exist.
    // FIXME: This should be styledElement since in JS getComputedStyle only works
    // on Elements, but right now editing creates these for text nodes. We should fix that.
    Node* styledNode() const;

    RefPtr<CSSValue> svgPropertyValue(CSSPropertyID, EUpdateLayout) const;
    RefPtr<SVGPaint> adjustSVGPaintForCurrentColor(RefPtr<SVGPaint>&&, const RenderStyle*) const;

    static Ref<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle&, AdjustPixelValuesForComputedStyle = AdjustPixelValues);
    RefPtr<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle*, const Color&) const;

    RefPtr<CSSValueList> getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand&) const;
    RefPtr<CSSValueList> getCSSPropertyValuesForSidesShorthand(const StylePropertyShorthand&) const;
    RefPtr<CSSValueList> getBackgroundShorthandValue() const;
    RefPtr<CSSValueList> getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand&) const;

    RefPtr<Node> m_node;
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
    RefPtr<CSSValue> getPropertyCSSValue(const String& propertyName) final;
    String getPropertyValue(const String& propertyName) final;
    String getPropertyPriority(const String& propertyName) final;
    String getPropertyShorthand(const String& propertyName) final;
    bool isPropertyImplicit(const String& propertyName) final;
    void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode&) final;
    String removeProperty(const String& propertyName, ExceptionCode&) final;
    String cssText() const final;
    void setCssText(const String&, ExceptionCode&) final;
    RefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) final;
    String getPropertyValueInternal(CSSPropertyID) final;
    bool setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionCode&) final;
    Ref<MutableStyleProperties> copyProperties() const final;

    RefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID, EUpdateLayout = UpdateLayout) const;

    Ref<Element> m_element;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
    unsigned m_refCount;
};

} // namespace WebCore

#endif // CSSComputedStyleDeclaration_h
