/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "CSSStyleDeclaration.h"

#include "CSSMutableStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSRule.h"
#include "DeprecatedValueList.h"
#include "ksvgcssproperties.h"

// Not in any header, so just declare it here for now.
WebCore::String getPropertyName(unsigned short id);

namespace WebCore {

// Defined in CSSGrammar.y, but not in any header, so just declare it here for now.
int getPropertyID(const char* str, int len);

static int propertyID(const String& s)
{
    char buffer[maxCSSPropertyNameLength];

    unsigned len = s.length();
    if (len > maxCSSPropertyNameLength)
        return 0;

    for (unsigned i = 0; i != len; ++i) {
        UChar c = s[i];
        if (c == 0 || c >= 0x7F)
            return 0; // illegal character
        buffer[i] = c;
    }

    int id = getPropertyID(buffer, len);
#if SVG_SUPPORT
    if (id == 0)
        id = SVG::getSVGCSSPropertyID(buffer, len);
#endif
    return id;
}

CSSStyleDeclaration::CSSStyleDeclaration(CSSRule* parent)
    : StyleBase(parent)
{
}

bool CSSStyleDeclaration::isStyleDeclaration()
{
    return true;
}

PassRefPtr<CSSValue> CSSStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return 0;
    return getPropertyCSSValue(propID);
}

String CSSStyleDeclaration::getPropertyValue(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    return getPropertyValue(propID);
}

String CSSStyleDeclaration::getPropertyPriority(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    return getPropertyPriority(propID) ? "important" : "";
}

String CSSStyleDeclaration::getPropertyShorthand(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    int shorthandID = getPropertyShorthand(propID);
    if (!shorthandID)
        return String();
    return getPropertyName(shorthandID);
}

bool CSSStyleDeclaration::isPropertyImplicit(const String &propertyName)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return false;
    return isPropertyImplicit(propID);
}

void CSSStyleDeclaration::setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode& ec)
{
    int propID = propertyID(propertyName);
    if (!propID) // set exception?
        return;
    bool important = priority.find("important", 0, false) != -1;
    setProperty(propID, value, important, ec);
}

String CSSStyleDeclaration::removeProperty(const String& propertyName, ExceptionCode& ec)
{
    int propID = propertyID(propertyName);
    if (!propID)
        return String();
    return removeProperty(propID, ec);
}

bool CSSStyleDeclaration::isPropertyName(const String &propertyName)
{
    return propertyID(propertyName);
}

CSSRule *CSSStyleDeclaration::parentRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRule *>(parent()) : 0;
}

void CSSStyleDeclaration::diff(CSSMutableStyleDeclaration *style) const
{
    if (!style)
        return;

    DeprecatedValueList<int> properties;
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it(style->valuesIterator()); it != end; ++it) {
        const CSSProperty& property = *it;
        RefPtr<CSSValue> value = getPropertyCSSValue(property.id());
        if (value && (value->cssText() == property.value()->cssText()))
            properties.append(property.id());
    }
    
    for (DeprecatedValueListIterator<int> it(properties.begin()); it != properties.end(); ++it)
        style->removeProperty(*it);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSStyleDeclaration::copyPropertiesInSet(const int *set, unsigned length) const
{
    DeprecatedValueList<CSSProperty> list;
    for (unsigned i = 0; i < length; i++) {
        RefPtr<CSSValue> value = getPropertyCSSValue(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value.release(), false));
    }
    return new CSSMutableStyleDeclaration(0, list);
}

}
