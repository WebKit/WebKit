/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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
class Node;
class RenderStyle;
class SVGPaint;
class ShadowData;
class StylePropertySet;
class StylePropertyShorthand;

#if ENABLE(CSS_SHADERS)
class CustomFilterNumberParameter;
class CustomFilterParameter;
#endif

enum EUpdateLayout { DoNotUpdateLayout = false, UpdateLayout = true };

class CSSComputedStyleDeclaration : public CSSStyleDeclaration {
public:
    static PassRefPtr<CSSComputedStyleDeclaration> create(PassRefPtr<Node> node, bool allowVisitedStyle = false, const String& pseudoElementName = String())
    {
        return adoptRef(new CSSComputedStyleDeclaration(node, allowVisitedStyle, pseudoElementName));
    }
    virtual ~CSSComputedStyleDeclaration();

    virtual void ref() OVERRIDE;
    virtual void deref() OVERRIDE;

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    String getPropertyValue(CSSPropertyID) const;
    bool getPropertyPriority(CSSPropertyID) const;

    virtual PassRefPtr<StylePropertySet> copy() const;
    virtual PassRefPtr<StylePropertySet> makeMutable();

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID, EUpdateLayout) const;
    PassRefPtr<CSSValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
#if ENABLE(SVG)
    PassRefPtr<CSSValue> getSVGPropertyCSSValue(CSSPropertyID, EUpdateLayout) const;
#endif

    PassRefPtr<StylePropertySet> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;

private:
    CSSComputedStyleDeclaration(PassRefPtr<Node>, bool allowVisitedStyle, const String&);

    // CSSOM functions. Don't make these public.
    virtual CSSRule* parentRule() const;
    virtual unsigned length() const;
    virtual String item(unsigned index) const;
    virtual PassRefPtr<CSSValue> getPropertyCSSValue(const String& propertyName);
    virtual String getPropertyValue(const String& propertyName);
    virtual String getPropertyPriority(const String& propertyName);
    virtual String getPropertyShorthand(const String& propertyName);
    virtual bool isPropertyImplicit(const String& propertyName);
    virtual void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode&);
    virtual String removeProperty(const String& propertyName, ExceptionCode&);
    virtual String cssText() const;
    virtual void setCssText(const String&, ExceptionCode&);
    virtual PassRefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID);
    virtual String getPropertyValueInternal(CSSPropertyID);
    virtual void setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionCode&);

    virtual bool cssPropertyMatches(const CSSProperty*) const;

    PassRefPtr<CSSValue> valueForShadow(const ShadowData*, CSSPropertyID, RenderStyle*) const;
    PassRefPtr<CSSPrimitiveValue> currentColorOrValidColor(RenderStyle*, const Color&) const;
#if ENABLE(SVG)
    PassRefPtr<SVGPaint> adjustSVGPaintForCurrentColor(PassRefPtr<SVGPaint>, RenderStyle*) const;
#endif

#if ENABLE(CSS_SHADERS)
    PassRefPtr<CSSValue> valueForCustomFilterNumberParameter(const CustomFilterNumberParameter*) const;
    PassRefPtr<CSSValue> valueForCustomFilterParameter(const CustomFilterParameter*) const;
#endif

#if ENABLE(CSS_FILTERS)
    PassRefPtr<CSSValue> valueForFilter(RenderStyle*) const;
#endif

    PassRefPtr<CSSValueList> getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand&) const;
    PassRefPtr<CSSValueList> getCSSPropertyValuesForSidesShorthand(const StylePropertyShorthand&) const;

    RefPtr<Node> m_node;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
    unsigned m_refCount;
};

} // namespace WebCore

#endif // CSSComputedStyleDeclaration_h
