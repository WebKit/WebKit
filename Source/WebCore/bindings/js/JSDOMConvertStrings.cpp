/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSDOMConvertStrings.h"

#include "JSDOMExceptionHandling.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>


namespace WebCore {
using namespace JSC;

static inline String stringToByteString(ExecState& state, JSC::ThrowScope& scope, String&& string)
{
    if (!string.isAllLatin1()) {
        throwTypeError(&state, scope);
        return { };
    }

    return WTFMove(string);
}

String identifierToByteString(ExecState& state, const Identifier& identifier)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = identifier.string();
    return stringToByteString(state, scope, WTFMove(string));
}

String valueToByteString(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });

    return stringToByteString(state, scope, WTFMove(string));
}

static inline bool hasUnpairedSurrogate(StringView string)
{
    // Fast path for 8-bit strings; they can't have any surrogates.
    if (string.is8Bit())
        return false;
    for (auto codePoint : string.codePoints()) {
        if (U_IS_SURROGATE(codePoint))
            return true;
    }
    return false;
}

static inline String stringToUSVString(String&& string)
{
    // Fast path for the case where there are no unpaired surrogates.
    if (!hasUnpairedSurrogate(string))
        return WTFMove(string);

    // Slow path: http://heycam.github.io/webidl/#dfn-obtain-unicode
    // Replaces unpaired surrogates with the replacement character.
    StringBuilder result;
    result.reserveCapacity(string.length());
    StringView view { string };
    for (auto codePoint : view.codePoints()) {
        if (U_IS_SURROGATE(codePoint))
            result.append(replacementCharacter);
        else
            result.appendCharacter(codePoint);
    }
    return result.toString();
}

String identifierToUSVString(ExecState&, const Identifier& identifier)
{
    auto string = identifier.string();
    return stringToUSVString(WTFMove(string));
}

String valueToUSVString(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });

    return stringToUSVString(WTFMove(string));
}

} // namespace WebCore
