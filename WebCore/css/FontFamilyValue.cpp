/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "FontFamilyValue.h"

namespace WebCore {

// FIXME: This appears identical to isCSSTokenizerIdentifier from CSSPrimitiveValue.cpp, we should use a single function.
static bool isValidCSSIdentifier(const String& string)
{
    unsigned length = string.length();
    if (!length)
        return false;

    const UChar* characters = string.characters();
    UChar c = characters[0];
    if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c >= 0x80))
        return false;

    for (unsigned i = 1; i < length; ++i) {
        c = characters[i];
        if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c >= 0x80))
            return false;
    }

    return true;
}

// Quotes the string if it needs quoting.
// We use single quotes because serialization code uses double quotes, and it's nice to
// avoid having to turn all the quote marks into &quot; as we would have to.
static String quoteStringIfNeeded(const String& string)
{
    if (isValidCSSIdentifier(string))
        return string;

    // FIXME: Also need to transform control characters (00-1F) into \ sequences.
    // FIXME: This is inefficient -- should use a Vector<UChar> instead.
    String quotedString = string;
    quotedString.replace('\\', "\\\\");
    quotedString.replace('\'', "\\'");
    return "'" + quotedString + "'";
}

FontFamilyValue::FontFamilyValue(const String& familyName)
    : CSSPrimitiveValue(String(), CSS_STRING)
    , m_familyName(familyName)
{
    // If there is anything in parentheses or square brackets at the end, delete it.
    // FIXME: Do we really need this? The original code mentioned "a language tag in
    // braces at the end" and "[Xft] qualifiers", but it's not clear either of those
    // is in active use on the web.
    unsigned length = m_familyName.length();
    while (length >= 3) {
        UChar startCharacter = 0;
        switch (m_familyName[length - 1]) {
            case ']':
                startCharacter = '[';
                break;
            case ')':
                startCharacter = '(';
                break;
        }
        if (!startCharacter)
            break;
        unsigned first = 0;
        for (unsigned i = length - 2; i > 0; --i) {
            if (m_familyName[i - 1] == ' ' && m_familyName[i] == startCharacter)
                first = i - 1;
        }
        if (!first)
            break;
        length = first;
    }
    m_familyName.truncate(length);
}

void FontFamilyValue::appendSpaceSeparated(const UChar* characters, unsigned length)
{
    m_familyName.append(' ');
    m_familyName.append(characters, length);
}

String FontFamilyValue::cssText() const
{
    return quoteStringIfNeeded(m_familyName);
}

}
