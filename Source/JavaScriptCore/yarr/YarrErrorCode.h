/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

namespace JSC {

class CallFrame;
class JSGlobalObject;
class JSObject;

namespace Yarr {

enum class ErrorCode : uint8_t {
    NoError = 0,

    // A hard error means that no matter what string the RegExp is evaluated on, it will
    // always fail. A SyntaxError is a hard error because the RegExp will never succeed no
    // matter what string it is run on. An OOME is not a hard error because the RegExp may
    // succeed when run on a different string.

    // The following are hard errors.
    PatternTooLarge,
    QuantifierOutOfOrder,
    QuantifierWithoutAtom,
    QuantifierTooLarge,
    QuantifierIncomplete,
    CantQuantifyAtom,
    MissingParentheses,
    BracketUnmatched,
    ParenthesesUnmatched,
    ParenthesesTypeInvalid,
    InvalidGroupName,
    DuplicateGroupName,
    CharacterClassUnmatched,
    CharacterClassRangeOutOfOrder,
    CharacterClassRangeInvalid,
    ClassStringDisjunctionUnmatched,
    EscapeUnterminated,
    InvalidUnicodeEscape,
    InvalidUnicodeCodePointEscape,
    InvalidBackreference,
    InvalidNamedBackReference,
    InvalidIdentityEscape,
    InvalidOctalEscape,
    InvalidControlLetterEscape,
    InvalidUnicodePropertyExpression,
    OffsetTooLarge,
    InvalidRegularExpressionFlags,
    InvalidClassSetOperation,
    NegatedClassSetMayContainStrings,
    InvalidClassSetCharacter,

    // The following are NOT hard errors.
    TooManyDisjunctions, // we ran out stack compiling.
};

JS_EXPORT_PRIVATE ASCIILiteral errorMessage(ErrorCode);
inline bool hasError(ErrorCode errorCode)
{
    return errorCode != ErrorCode::NoError;
}

inline bool hasHardError(ErrorCode errorCode)
{
    // See comment in the enum class ErrorCode above for the definition of hard errors.
    return hasError(errorCode) && errorCode < ErrorCode::TooManyDisjunctions;
}
JS_EXPORT_PRIVATE JSObject* errorToThrow(JSGlobalObject*, ErrorCode);

} } // namespace JSC::Yarr
