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

#ifndef CSSMutableStyleDeclaration_h
#define CSSMutableStyleDeclaration_h

#include "CSSStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "KURLHash.h"
#include "PlatformString.h"
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class PropertySetCSSStyleDeclaration;
class StyledElement;

class StylePropertySet : public WTF::RefCountedBase {
public:
    ~StylePropertySet();

    static PassRefPtr<StylePropertySet> create()
    {
        return adoptRef(new StylePropertySet);
    }
    static PassRefPtr<StylePropertySet> create(CSSRule* parentRule)
    {
        return adoptRef(new StylePropertySet(parentRule));
    }
    static PassRefPtr<StylePropertySet> create(CSSRule* parentRule, const CSSProperty* const* properties, int numProperties)
    {
        return adoptRef(new StylePropertySet(parentRule, properties, numProperties));
    }
    static PassRefPtr<StylePropertySet> create(const Vector<CSSProperty>& properties)
    {
        return adoptRef(new StylePropertySet(0, properties));
    }
    static PassRefPtr<StylePropertySet> createInline(StyledElement* element)
    { 
        return adoptRef(new StylePropertySet(element));
    }

    void deref();

    unsigned propertyCount() const { return m_properties.size(); }
    bool isEmpty() const { return m_properties.isEmpty(); }
    const CSSProperty& propertyAt(unsigned index) const { return m_properties[index]; }

    PassRefPtr<CSSValue> getPropertyCSSValue(int propertyID) const;
    String getPropertyValue(int propertyID) const;
    bool propertyIsImportant(int propertyID) const;
    int getPropertyShorthand(int propertyID) const;
    bool isPropertyImplicit(int propertyID) const;

    bool setProperty(int propertyID, int value, bool important = false) { return setProperty(propertyID, value, important, true); }
    bool setProperty(int propertyId, double value, CSSPrimitiveValue::UnitTypes unit, bool important = false) { return setProperty(propertyId, value, unit, important, true); }
    bool setProperty(int propertyID, const String& value, bool important = false) { return setProperty(propertyID, value, important, true); }
    void setProperty(const CSSProperty&, CSSProperty* slot = 0);
    
    void removeProperty(int propertyID) { removeProperty(propertyID, true, false); }
    String removeProperty(int propertyID, bool notifyChanged, bool returnText);

    // The following parses an entire new style declaration.
    void parseDeclaration(const String& styleDeclaration);

    // Besides adding the properties, this also removes any existing properties with these IDs.
    // It does no notification since it's called by the parser.
    void addParsedProperties(const CSSProperty* const *, int numProperties);
    // This does no change notifications since it's only called by createMarkup.
    void addParsedProperty(const CSSProperty&);

    PassRefPtr<StylePropertySet> copyBlockProperties() const;
    void removeBlockProperties();
    void removePropertiesInSet(const int* set, unsigned length) { removePropertiesInSet(set, length, true); }

    void merge(const StylePropertySet*, bool argOverridesOnConflict = true);

    void setStrictParsing(bool b) { m_strictParsing = b; }
    bool useStrictParsing() const { return m_strictParsing; }
    bool isInlineStyleDeclaration() const { return m_isInlineStyleDeclaration; }

    void addSubresourceStyleURLs(ListHashSet<KURL>&);

    PassRefPtr<StylePropertySet> copy() const;
    // Used by StyledElement::copyNonAttributeProperties().
    void copyPropertiesFrom(const StylePropertySet&);

    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    PassRefPtr<StylePropertySet> copyPropertiesInSet(const int* set, unsigned length) const;

    CSSRule* parentRuleInternal() const { return m_isInlineStyleDeclaration ? 0 : m_parent.rule; }
    void clearParentRule() { ASSERT(!m_isInlineStyleDeclaration); m_parent.rule = 0; }
    
    StyledElement* parentElement() const { ASSERT(m_isInlineStyleDeclaration); return m_parent.element; }
    void clearParentElement() { ASSERT(m_isInlineStyleDeclaration); m_parent.element = 0; }
    
    CSSStyleSheet* contextStyleSheet() const;
    
    String asText() const;
    
    CSSStyleDeclaration* ensureCSSStyleDeclaration() const;

private:
    StylePropertySet();
    StylePropertySet(CSSRule* parentRule);
    StylePropertySet(CSSRule* parentRule, const Vector<CSSProperty>&);
    StylePropertySet(CSSRule* parentRule, const CSSProperty* const *, int numProperties);
    StylePropertySet(StyledElement*);

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

    bool setProperty(int propertyID, int value, bool important, bool notifyChanged);
    bool setProperty(int propertyId, double value, CSSPrimitiveValue::UnitTypes, bool important, bool notifyChanged);
    bool setProperty(int propertyID, const String& value, bool important, bool notifyChanged);
    bool removeShorthandProperty(int propertyID, bool notifyChanged);
    bool removePropertiesInSet(const int* set, unsigned length, bool notifyChanged);
    bool propertyMatches(const CSSProperty*) const;

    const CSSProperty* findPropertyWithId(int propertyId) const;
    CSSProperty* findPropertyWithId(int propertyId);

    Vector<CSSProperty, 4> m_properties;

    bool m_strictParsing : 1;
    bool m_isInlineStyleDeclaration : 1;

    union Parent {
        Parent(CSSRule* rule) : rule(rule) { }
        Parent(StyledElement* element) : element(element) { }
        CSSRule* rule;
        StyledElement* element;
    } m_parent;
    
    mutable RefPtr<PropertySetCSSStyleDeclaration> m_cssStyleDeclaration;
    
    friend class PropertySetCSSStyleDeclaration;
};

} // namespace WebCore

#endif // CSSMutableStyleDeclaration_h
