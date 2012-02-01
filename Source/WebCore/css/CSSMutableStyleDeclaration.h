/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

class StyledElement;

class CSSMutableStyleDeclaration : public CSSStyleDeclaration {
public:
    virtual ~CSSMutableStyleDeclaration();

    static PassRefPtr<CSSMutableStyleDeclaration> create()
    {
        return adoptRef(new CSSMutableStyleDeclaration);
    }
    static PassRefPtr<CSSMutableStyleDeclaration> create(CSSRule* parentRule)
    {
        return adoptRef(new CSSMutableStyleDeclaration(parentRule));
    }
    static PassRefPtr<CSSMutableStyleDeclaration> create(CSSRule* parentRule, const CSSProperty* const* properties, int numProperties)
    {
        return adoptRef(new CSSMutableStyleDeclaration(parentRule, properties, numProperties));
    }
    static PassRefPtr<CSSMutableStyleDeclaration> create(const Vector<CSSProperty>& properties)
    {
        return adoptRef(new CSSMutableStyleDeclaration(0, properties));
    }
    static PassRefPtr<CSSMutableStyleDeclaration> createInline(StyledElement* element)
    { 
        return adoptRef(new CSSMutableStyleDeclaration(element));
    }

    unsigned propertyCount() const { return m_properties.size(); }
    bool isEmpty() const { return m_properties.isEmpty(); }
    const CSSProperty& propertyAt(unsigned index) const { return m_properties[index]; }

    PassRefPtr<CSSValue> getPropertyCSSValue(int propertyID) const;
    String getPropertyValue(int propertyID) const;
    bool propertyIsImportant(int propertyID) const;
    int getPropertyShorthand(int propertyID) const;
    bool isPropertyImplicit(int propertyID) const;

    virtual PassRefPtr<CSSMutableStyleDeclaration> copy() const;

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

    PassRefPtr<CSSMutableStyleDeclaration> copyBlockProperties() const;
    void removeBlockProperties();
    void removePropertiesInSet(const int* set, unsigned length) { removePropertiesInSet(set, length, true); }

    void merge(const CSSMutableStyleDeclaration*, bool argOverridesOnConflict = true);

    void setStrictParsing(bool b) { m_strictParsing = b; }
    bool useStrictParsing() const { return m_strictParsing; }

    void addSubresourceStyleURLs(ListHashSet<KURL>&);

    // Used by StyledElement::copyNonAttributeProperties().
    void copyPropertiesFrom(const CSSMutableStyleDeclaration&);

    void removeEquivalentProperties(const CSSStyleDeclaration*);

    PassRefPtr<CSSMutableStyleDeclaration> copyPropertiesInSet(const int* set, unsigned length) const;

    String asText() const;

private:
    CSSMutableStyleDeclaration();
    CSSMutableStyleDeclaration(CSSRule* parentRule);
    CSSMutableStyleDeclaration(CSSRule* parentRule, const Vector<CSSProperty>&);
    CSSMutableStyleDeclaration(CSSRule* parentRule, const CSSProperty* const *, int numProperties);
    CSSMutableStyleDeclaration(StyledElement*);

    virtual PassRefPtr<CSSMutableStyleDeclaration> makeMutable();

    // CSSOM functions. Don't make these public.
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

    virtual bool cssPropertyMatches(const CSSProperty*) const;

    const CSSProperty* findPropertyWithId(int propertyId) const;
    CSSProperty* findPropertyWithId(int propertyId);

    Vector<CSSProperty, 4> m_properties;
};

} // namespace WebCore

#endif // CSSMutableStyleDeclaration_h
