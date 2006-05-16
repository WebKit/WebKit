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
#include "FontFamilyValue.h"

#include "PlatformString.h"
#include "RegularExpression.h"

namespace WebCore {

// Quotes the string if it needs quoting.
// We use single quotes for now beause markup.cpp uses double quotes.
static String quoteStringIfNeeded(const String &string)
{
    // For now, just do this for strings that start with "#" to fix Korean font names that start with "#".
    // Post-Tiger, we should isLegalIdentifier instead after working out all the ancillary issues.
    if (string[0] != '#')
        return string;

    // FIXME: Also need to transform control characters into \ sequences.
    String s = string;
    s.replace('\\', "\\\\");
    s.replace('\'', "\\'");
    return "'" + s + "'";
}

FontFamilyValue::FontFamilyValue(const DeprecatedString& string)
    : CSSPrimitiveValue(String(), CSS_STRING)
{
    static const RegularExpression parenReg(" \\(.*\\)$");
    static const RegularExpression braceReg(" \\[.*\\]$");

    parsedFontName = string;
    // a language tag is often added in braces at the end. Remove it.
    parsedFontName.replace(parenReg, "");
    // remove [Xft] qualifiers
    parsedFontName.replace(braceReg, "");
}

String FontFamilyValue::cssText() const
{
    return quoteStringIfNeeded(parsedFontName);
}

}
