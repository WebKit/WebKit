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
#include "DeprecatedValueList.h"
#include "PlatformString.h"

namespace WebCore {

class CSSProperty;
class Node;

class CSSMutableStyleDeclaration : public CSSStyleDeclaration {
public:
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
    static PassRefPtr<CSSMutableStyleDeclaration> create(const DeprecatedValueList<CSSProperty>& properties, unsigned variableDependentValueCount)
    {
        return adoptRef(new CSSMutableStyleDeclaration(0, properties, variableDependentValueCount));
    }

    CSSMutableStyleDeclaration& operator=(const CSSMutableStyleDeclaration&);

    void setNode(Node* node) { m_node = node; }

    virtual bool isMutableStyleDeclaration() const { return true; }

    virtual String cssText() const;
    virtual void setCssText(const String&, ExceptionCode&);

    virtual unsigned length() const;
    virtual String item(unsigned index) const;

    virtual PassRefPtr<CSSValue> getPropertyCSSValue(int propertyID) const;
    virtual String getPropertyValue(int propertyID) const;
    virtual bool getPropertyPriority(int propertyID) const;
    virtual int getPropertyShorthand(int propertyID) const;
    virtual bool isPropertyImplicit(int propertyID) const;

    virtual void setProperty(int propertyId, const String& value, bool important, ExceptionCode&);
    virtual String removeProperty(int propertyID, ExceptionCode&);

    virtual PassRefPtr<CSSMutableStyleDeclaration> copy() const;

    DeprecatedValueListConstIterator<CSSProperty> valuesIterator() const { return m_values.begin(); }

    bool setProperty(int propertyID, int value, bool important = false, bool notifyChanged = true);
    bool setProperty(int propertyID, const String& value, bool important, bool notifyChanged, ExceptionCode&);
    bool setProperty(int propertyId, const String& value, bool important = false, bool notifyChanged = true)
    { 
        ExceptionCode ec;
        return setProperty(propertyId, value, important, notifyChanged, ec);
    }

    String removeProperty(int propertyID, bool notifyChanged, bool returnText, ExceptionCode&);
    void removeProperty(int propertyID, bool notifyChanged = true)
    {
        ExceptionCode ec;
        removeProperty(propertyID, notifyChanged, false, ec);
    }
 
    // setLengthProperty treats integers as pixels! (Needed for conversion of HTML attributes.)
    void setLengthProperty(int propertyId, const String& value, bool important, bool multiLength = false);
    void setStringProperty(int propertyId, const String& value, CSSPrimitiveValue::UnitTypes, bool important = false); // parsed string value
    void setImageProperty(int propertyId, const String& url, bool important = false);
 
    // The following parses an entire new style declaration.
    void parseDeclaration(const String& styleDeclaration);

    // Besides adding the properties, this also removes any existing properties with these IDs.
    // It does no notification since it's called by the parser.
    void addParsedProperties(const CSSProperty* const *, int numProperties);
    // This does no change notifications since it's only called by createMarkup.
    void addParsedProperty(const CSSProperty&);
 
    PassRefPtr<CSSMutableStyleDeclaration> copyBlockProperties() const;
    void removeBlockProperties();
    void removePropertiesInSet(const int* set, unsigned length, bool notifyChanged = true);

    void merge(CSSMutableStyleDeclaration*, bool argOverridesOnConflict = true);

    bool hasVariableDependentValue() const { return m_variableDependentValueCount > 0; }
    
    void setStrictParsing(bool b) { m_strictParsing = b; }
    bool useStrictParsing() const { return m_strictParsing; }
    
protected:
    CSSMutableStyleDeclaration(CSSRule* parentRule);

private:
    CSSMutableStyleDeclaration();
    CSSMutableStyleDeclaration(CSSRule* parentRule, const DeprecatedValueList<CSSProperty>&, unsigned variableDependentValueCount);
    CSSMutableStyleDeclaration(CSSRule* parentRule, const CSSProperty* const *, int numProperties);

    virtual PassRefPtr<CSSMutableStyleDeclaration> makeMutable();

    void setChanged();

    String getShorthandValue(const int* properties, int number) const;
    String getCommonValue(const int* properties, int number) const;
    String getLayeredShorthandValue(const int* properties, unsigned number) const;
    String get4Values(const int* properties) const;
 
    DeprecatedValueList<CSSProperty> m_values;
    Node* m_node;
    unsigned m_variableDependentValueCount : 31;
    bool m_strictParsing : 1;
};

} // namespace WebCore

#endif // CSSMutableStyleDeclaration_h
