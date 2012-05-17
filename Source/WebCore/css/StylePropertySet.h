/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef StylePropertySet_h
#define StylePropertySet_h

#include "CSSParserMode.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSRule;
class CSSStyleDeclaration;
class KURL;
class PropertySetCSSStyleDeclaration;
class StyledElement;
class StylePropertyShorthand;
class StyleSheetInternal;

class StylePropertySet : public RefCounted<StylePropertySet> {
public:
    ~StylePropertySet();

    static PassRefPtr<StylePropertySet> create(CSSParserMode cssParserMode = CSSQuirksMode)
    {
        return adoptRef(new StylePropertySet(cssParserMode));
    }
    static PassRefPtr<StylePropertySet> create(const CSSProperty* properties, int numProperties, CSSParserMode cssParserMode)
    {
        return adoptRef(new StylePropertySet(properties, numProperties, cssParserMode));
    }
    static PassRefPtr<StylePropertySet> create(const Vector<CSSProperty>& properties)
    {
        return adoptRef(new StylePropertySet(properties));
    }

    unsigned propertyCount() const { return m_properties.size(); }
    bool isEmpty() const { return m_properties.isEmpty(); }
    const CSSProperty& propertyAt(unsigned index) const { return m_properties[index]; }

    void shrinkToFit() { m_properties.shrinkToFit(); }

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    String getPropertyValue(CSSPropertyID) const;
    bool propertyIsImportant(CSSPropertyID) const;
    CSSPropertyID getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important = false, StyleSheetInternal* contextStyleSheet = 0);
    void setProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, int identifier, bool important = false);
    void setProperty(const CSSProperty&, CSSProperty* slot = 0);
    
    bool removeProperty(CSSPropertyID, String* returnText = 0);

    void parseDeclaration(const String& styleDeclaration, StyleSheetInternal* contextStyleSheet);

    void addParsedProperties(const CSSProperty*, int numProperties);
    void addParsedProperty(const CSSProperty&);

    PassRefPtr<StylePropertySet> copyBlockProperties() const;
    void removeBlockProperties();
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);

    void merge(const StylePropertySet*, bool argOverridesOnConflict = true);

    void setCSSParserMode(CSSParserMode cssParserMode) { m_cssParserMode = cssParserMode; }
    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    void addSubresourceStyleURLs(ListHashSet<KURL>&, StyleSheetInternal* contextStyleSheet);

    PassRefPtr<StylePropertySet> copy() const;
    // Used by StyledElement::copyNonAttributeProperties().
    void copyPropertiesFrom(const StylePropertySet&);

    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    PassRefPtr<StylePropertySet> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;
    
    void clearParentElement(StyledElement*);

    CSSStyleDeclaration* ensureCSSStyleDeclaration() const;
    CSSStyleDeclaration* ensureInlineCSSStyleDeclaration(const StyledElement* parentElement) const;
    
    // FIXME: Expand the concept of mutable/immutable StylePropertySet.
    bool isMutable() const { return m_ownsCSSOMWrapper; }

    static unsigned averageSizeInBytes();

#ifndef NDEBUG
    void showStyle();
#endif
    
private:
    StylePropertySet(CSSParserMode);
    StylePropertySet(const Vector<CSSProperty>&);
    StylePropertySet(const StylePropertySet&);
    StylePropertySet(const CSSProperty*, int numProperties, CSSParserMode);

    void setNeedsStyleRecalc();

    String getShorthandValue(const StylePropertyShorthand&) const;
    String getCommonValue(const StylePropertyShorthand&) const;
    enum CommonValueMode { OmitUncommonValues, ReturnNullOnUncommonValues };
    String borderPropertyValue(CommonValueMode) const;
    String getLayeredShorthandValue(const StylePropertyShorthand&) const;
    String get4Values(const StylePropertyShorthand&) const;
    String borderSpacingValue(const StylePropertyShorthand&) const;
    String fontValue() const;
    bool appendFontLonghandValueIfExplicit(CSSPropertyID, StringBuilder& result) const;

    bool removeShorthandProperty(CSSPropertyID);
    bool propertyMatches(const CSSProperty*) const;

    const CSSProperty* findPropertyWithId(CSSPropertyID) const;
    CSSProperty* findPropertyWithId(CSSPropertyID);

    Vector<CSSProperty, 4> m_properties;

    unsigned m_cssParserMode : 2;
    mutable unsigned m_ownsCSSOMWrapper : 1;
    
    friend class PropertySetCSSStyleDeclaration;
};

} // namespace WebCore

#endif // StylePropertySet_h
