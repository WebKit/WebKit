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
    ComputedStyleExtractor(PassRefPtr<Node>, bool allowVisitedStyle = false, PseudoId = NOPSEUDO);

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
    RefPtr<SVGPaint> adjustSVGPaintForCurrentColor(PassRefPtr<SVGPaint>, RenderStyle*) const;

    static Ref<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, const RenderStyle&, AdjustPixelValuesForComputedStyle = AdjustPixelValues);
    RefPtr<CSSPrimitiveValue> currentColorOrValidColor(RenderStyle*, const Color&) const;

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
    static Ref<CSSComputedStyleDeclaration> create(PassRefPtr<Node> node, bool allowVisitedStyle = false, const String& pseudoElementName = String())
    {
        return adoptRef(*new CSSComputedStyleDeclaration(node, allowVisitedStyle, pseudoElementName));
    }
    virtual ~CSSComputedStyleDeclaration();

    WEBCORE_EXPORT void ref() override;
    WEBCORE_EXPORT void deref() override;

    String getPropertyValue(CSSPropertyID) const;

private:
    WEBCORE_EXPORT CSSComputedStyleDeclaration(PassRefPtr<Node>, bool allowVisitedStyle, const String&);

    // CSSOM functions. Don't make these public.
    CSSRule* parentRule() const override;
    unsigned length() const override;
    String item(unsigned index) const override;
    RefPtr<CSSValue> getPropertyCSSValue(const String& propertyName) override;
    String getPropertyValue(const String& propertyName) override;
    String getPropertyPriority(const String& propertyName) override;
    String getPropertyShorthand(const String& propertyName) override;
    bool isPropertyImplicit(const String& propertyName) override;
    void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode&) override;
    String removeProperty(const String& propertyName, ExceptionCode&) override;
    String cssText() const override;
    void setCssText(const String&, ExceptionCode&) override;
    RefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) override;
    String getPropertyValueInternal(CSSPropertyID) override;
    bool setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionCode&) override;
    Ref<MutableStyleProperties> copyProperties() const override;

    RefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID, EUpdateLayout = UpdateLayout) const;

    RefPtr<Node> m_node;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
    unsigned m_refCount;
};

} // namespace WebCore

#endif // CSSComputedStyleDeclaration_h
