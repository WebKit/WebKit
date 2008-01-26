/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "HTMLFontElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

HTMLFontElement::HTMLFontElement(Document* doc)
    : HTMLElement(fontTag, doc)
{
}

HTMLFontElement::~HTMLFontElement()
{
}

// Allows leading spaces.
// Allows trailing nonnumeric characters.
// Returns 10 for any size greater than 9.
static bool parseFontSizeNumber(const String& s, int& size)
{
    unsigned pos = 0;
    
    // Skip leading spaces.
    while (isSpaceOrNewline(s[pos]))
        ++pos;
    
    // Skip a plus or minus.
    bool sawPlus = false;
    bool sawMinus = false;
    if (s[pos] == '+') {
        ++pos;
        sawPlus = true;
    } else if (s[pos] == '-') {
        ++pos;
        sawMinus = true;
    }
    
    // Parse a single digit.
    if (!Unicode::isDigit(s[pos]))
        return false;
    int num = Unicode::digitValue(s[pos++]);
    
    // Check for an additional digit.
    if (Unicode::isDigit(s[pos]))
        num = 10;
    
    if (sawPlus) {
        size = num + 3;
        return true;
    }
    
    // Don't return 0 (which means 3) or a negative number (which means the same as 1).
    if (sawMinus) {
        size = num == 1 ? 2 : 1;
        return true;
    }
    
    size = num;
    return true;
}

bool HTMLFontElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == sizeAttr ||
        attrName == colorAttr ||
        attrName == faceAttr) {
        result = eUniversal;
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

bool HTMLFontElement::cssValueFromFontSizeNumber(const String& s, int& size)
{
    int num;
    if (!parseFontSizeNumber(s, num))
        return false;
        
    switch (num) {
        case 2: 
            size = CSS_VAL_SMALL; 
            break;
        case 0: // treat 0 the same as 3, because people expect it to be between -1 and +1
        case 3: 
            size = CSS_VAL_MEDIUM; 
            break;
        case 4: 
            size = CSS_VAL_LARGE; 
            break;
        case 5: 
            size = CSS_VAL_X_LARGE; 
            break;
        case 6: 
            size = CSS_VAL_XX_LARGE; 
            break;
        default:
            if (num > 6)
                size = CSS_VAL__WEBKIT_XXX_LARGE;
            else
                size = CSS_VAL_X_SMALL;
    }
    return true;
}

void HTMLFontElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == sizeAttr) {
        int size;
        if (cssValueFromFontSizeNumber(attr->value(), size))
            addCSSProperty(attr, CSS_PROP_FONT_SIZE, size);
    } else if (attr->name() == colorAttr) {
        addCSSColor(attr, CSS_PROP_COLOR, attr->value());
    } else if (attr->name() == faceAttr) {
        addCSSProperty(attr, CSS_PROP_FONT_FAMILY, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLFontElement::color() const
{
    return getAttribute(colorAttr);
}

void HTMLFontElement::setColor(const String& value)
{
    setAttribute(colorAttr, value);
}

String HTMLFontElement::face() const
{
    return getAttribute(faceAttr);
}

void HTMLFontElement::setFace(const String& value)
{
    setAttribute(faceAttr, value);
}

String HTMLFontElement::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLFontElement::setSize(const String& value)
{
    setAttribute(sizeAttr, value);
}

}
