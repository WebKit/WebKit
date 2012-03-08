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

#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSRule;
class CSSStyleDeclaration;
class KURL;
class PropertySetCSSStyleDeclaration;
class StyledElement;

class StylePropertySet : public RefCounted<StylePropertySet> {
public:
    ~StylePropertySet();

    static PassRefPtr<StylePropertySet> create()
    {
        return adoptRef(new StylePropertySet);
    }
    static PassRefPtr<StylePropertySet> create(const CSSProperty* const* properties, int numProperties, bool useStrictParsing)
    {
        return adoptRef(new StylePropertySet(properties, numProperties, useStrictParsing));
    }
    static PassRefPtr<StylePropertySet> create(const Vector<CSSProperty>& properties)
    {
        return adoptRef(new StylePropertySet(properties));
    }

    unsigned propertyCount() const { return m_properties.size(); }
    bool isEmpty() const { return m_properties.isEmpty(); }
    const CSSProperty& propertyAt(unsigned index) const { return m_properties[index]; }

    void shrinkToFit() { m_properties.shrinkToFit(); }

    PassRefPtr<CSSValue> getPropertyCSSValue(int propertyID) const;
    String getPropertyValue(int propertyID) const;
    bool propertyIsImportant(int propertyID) const;
    int getPropertyShorthand(int propertyID) const;
    bool isPropertyImplicit(int propertyID) const;

    // These expand shorthand properties into multiple properties.
    bool setProperty(int propertyID, const String& value, bool important = false, CSSStyleSheet* contextStyleSheet = 0);
    void setProperty(int propertyID, PassRefPtr<CSSValue>, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(int propertyID, int value, bool important = false, CSSStyleSheet* contextStyleSheet = 0);
    void setProperty(const CSSProperty&, CSSProperty* slot = 0);
    
    bool removeProperty(int propertyID, String* returnText = 0);

    void parseDeclaration(const String& styleDeclaration, CSSStyleSheet* contextStyleSheet);

    void addParsedProperties(const CSSProperty* const *, int numProperties);
    void addParsedProperty(const CSSProperty&);

    PassRefPtr<StylePropertySet> copyBlockProperties() const;
    void removeBlockProperties();
    bool removePropertiesInSet(const int* set, unsigned length);

    void merge(const StylePropertySet*, bool argOverridesOnConflict = true);

    void setStrictParsing(bool b) { m_strictParsing = b; }
    bool useStrictParsing() const { return m_strictParsing; }

    void addSubresourceStyleURLs(ListHashSet<KURL>&, CSSStyleSheet* contextStyleSheet);

    PassRefPtr<StylePropertySet> copy() const;
    // Used by StyledElement::copyNonAttributeProperties().
    void copyPropertiesFrom(const StylePropertySet&);

    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    PassRefPtr<StylePropertySet> copyPropertiesInSet(const int* set, unsigned length) const;
    
    String asText() const;
    
    void clearParentRule(CSSRule*);
    void clearParentElement(StyledElement*);

    CSSStyleDeclaration* ensureCSSStyleDeclaration() const;
    CSSStyleDeclaration* ensureRuleCSSStyleDeclaration(const CSSRule* parentRule) const;
    CSSStyleDeclaration* ensureInlineCSSStyleDeclaration(const StyledElement* parentElement) const;
    
    bool hasCSSOMWrapper() const { return m_hasCSSOMWrapper; }

private:
    StylePropertySet();
    StylePropertySet(const Vector<CSSProperty>&);
    StylePropertySet(const CSSProperty* const *, int numProperties, bool useStrictParsing);

    void setNeedsStyleRecalc();

    String getShorthandValue(const int* properties, size_t) const;
    String getCommonValue(const int* properties, size_t) const;
    String getLayeredShorthandValue(const int* properties, size_t) const;
    String get4Values(const int* properties) const;
    String borderSpacingValue(const int properties[2]) const;
    String fontValue() const;
    bool appendFontLonghandValueIfExplicit(int propertyID, StringBuilder& result) const;

    template<size_t size> String getShorthandValue(const int (&properties)[size]) const { return getShorthandValue(properties, size); }
    template<size_t size> String getCommonValue(const int (&properties)[size]) const { return getCommonValue(properties, size); }
    template<size_t size> String getLayeredShorthandValue(const int (&properties)[size]) const { return getLayeredShorthandValue(properties, size); }

    bool removeShorthandProperty(int propertyID);
    bool propertyMatches(const CSSProperty*) const;

    const CSSProperty* findPropertyWithId(int propertyID) const;
    CSSProperty* findPropertyWithId(int propertyID);

    Vector<CSSProperty, 4> m_properties;

    bool m_strictParsing : 1;
    mutable bool m_hasCSSOMWrapper : 1;
    
    friend class PropertySetCSSStyleDeclaration;
};

} // namespace WebCore

#endif // StylePropertySet_h
