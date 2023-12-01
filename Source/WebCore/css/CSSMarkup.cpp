/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2012, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "CSSMarkup.h"

#include "CSSParserIdioms.h"
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

template <typename CharacterType>
static inline bool isCSSTokenizerIdentifier(const CharacterType* characters, unsigned length)
{
    const CharacterType* end = characters + length;

    // -?
    if (characters != end && characters[0] == '-')
        ++characters;

    // {nmstart}
    if (characters == end || !isNameStartCodePoint(characters[0]))
        return false;
    ++characters;

    // {nmchar}*
    for (; characters != end; ++characters) {
        if (!isNameCodePoint(characters[0]))
            return false;
    }

    return true;
}

// "ident" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerIdentifier(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return false;

    if (string.is8Bit())
        return isCSSTokenizerIdentifier(string.characters8(), length);
    return isCSSTokenizerIdentifier(string.characters16(), length);
}

static void serializeCharacter(char32_t c, StringBuilder& appendTo)
{
    appendTo.append('\\', c);
}

static void serializeCharacterAsCodePoint(char32_t c, StringBuilder& appendTo)
{
    appendTo.append('\\', hex(c, Lowercase), ' ');
}

void serializeIdentifier(const String& identifier, StringBuilder& appendTo, bool skipStartChecks)
{
    bool isFirst = !skipStartChecks;
    bool isSecond = false;
    bool isFirstCharHyphen = false;
    unsigned index = 0;
    while (index < identifier.length()) {
        char32_t c = identifier.characterStartingAt(index);
        if (!c) {
            // Check for lone surrogate which characterStartingAt does not return.
            c = identifier[index];
        }

        index += U16_LENGTH(c);

        if (!c)
            appendTo.append(replacementCharacter);
        else if (c <= 0x1f || c == deleteCharacter || (0x30 <= c && c <= 0x39 && (isFirst || (isSecond && isFirstCharHyphen))))
            serializeCharacterAsCodePoint(c, appendTo);
        else if (c == hyphenMinus && isFirst && index == identifier.length())
            serializeCharacter(c, appendTo);
        else if (0x80 <= c || c == hyphenMinus || c == lowLine || (0x30 <= c && c <= 0x39) || (0x41 <= c && c <= 0x5a) || (0x61 <= c && c <= 0x7a))
            appendTo.append(c);
        else
            serializeCharacter(c, appendTo);

        if (isFirst) {
            isFirst = false;
            isSecond = true;
            isFirstCharHyphen = (c == hyphenMinus);
        } else if (isSecond)
            isSecond = false;
    }
}

void serializeString(const String& string, StringBuilder& appendTo)
{
    appendTo.append('"');

    unsigned index = 0;
    while (index < string.length()) {
        char32_t c = string.characterStartingAt(index);
        index += U16_LENGTH(c);

        if (c <= 0x1f || c == deleteCharacter)
            serializeCharacterAsCodePoint(c, appendTo);
        else if (c == quotationMark || c == reverseSolidus)
            serializeCharacter(c, appendTo);
        else
            appendTo.append(c);
    }

    appendTo.append('"');
}

String serializeString(const String& string)
{
    StringBuilder builder;
    serializeString(string, builder);
    return builder.toString();
}

String serializeURL(const String& string)
{
    StringBuilder builder;
    builder.append("url(");
    serializeString(string, builder);
    builder.append(')');
    return builder.toString();
}

String serializeFontFamily(const String& string)
{
    return isCSSTokenizerIdentifier(string) ? string : serializeString(string);
}

} // namespace WebCore
