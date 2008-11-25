/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               http://www.torchmobile.com/
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
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLVariables.h"

#include "PlatformString.h"
#include <wtf/ASCIICType.h>

namespace WebCore {

static bool isValidVariableCharacter(const UChar& character)
{
    return WTF::isASCIIAlpha(character)
           || WTF::isASCIIDigit(character)
           || character == '_';
}

static bool isValidVariableReferenceCharacter(const UChar& character)
{
    return character == '('
           || character == ')'
           || character == ':'
           || character == '$';
}

bool isValidVariableName(const String& name, bool isReference)
{
    if (name.isEmpty())
        return false;

    const UChar* characters = name.characters();
    bool isValid = (characters[0] == '_' || WTF::isASCIIAlpha(characters[0]));

    if (isReference && !isValid)
        isValid = isValidVariableReferenceCharacter(characters[0]);

    if (!isValid)
        return false;

    unsigned length = name.length();
    for (unsigned i = 1; i < length; ++i) {
        isValid = isValidVariableCharacter(characters[i]);

        if (isReference && !isValid)
            isValid = isValidVariableReferenceCharacter(characters[i]);

        if (!isValid)
            return false;
    }

    return true;
}

bool containsVariableReference(const String& text)
{
    int startPos = text.find('$');
    if (startPos == -1)
        return false;

    unsigned length = text.length();
    for (unsigned i = startPos + 1; i < length; ++i) {
        if (!isValidVariableReferenceCharacter(text[i]) && !isValidVariableCharacter(text[i]))
            return false;
    }

    return true;
}

String substituteVariableReferences(const String& variableReference, Document* document, WMLVariableEscapingMode escapeMode)
{
    // FIXME!
    return variableReference;
}

}

#endif
