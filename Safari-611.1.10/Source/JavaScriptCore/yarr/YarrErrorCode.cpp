/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "config.h"
#include "YarrErrorCode.h"

#include "Error.h"

namespace JSC { namespace Yarr {

const char* errorMessage(ErrorCode error)
{
#define REGEXP_ERROR_PREFIX "Invalid regular expression: "
    // The order of this array must match the ErrorCode enum.
    static const char* errorMessages[] = {
        nullptr,                                                                    // NoError
        REGEXP_ERROR_PREFIX "regular expression too large",                         // PatternTooLarge
        REGEXP_ERROR_PREFIX "numbers out of order in {} quantifier",                // QuantifierOutOfOrder
        REGEXP_ERROR_PREFIX "nothing to repeat",                                    // QuantifierWithoutAtom
        REGEXP_ERROR_PREFIX "number too large in {} quantifier",                    // QuantifierTooLarge
        REGEXP_ERROR_PREFIX "incomplete {} quantifier for Unicode pattern",         // QuantifierIncomplete
        REGEXP_ERROR_PREFIX "missing )",                                            // MissingParentheses
        REGEXP_ERROR_PREFIX "unmatched ] or } bracket for Unicode pattern",         // BracketUnmatched
        REGEXP_ERROR_PREFIX "unmatched parentheses",                                // ParenthesesUnmatched
        REGEXP_ERROR_PREFIX "unrecognized character after (?",                      // ParenthesesTypeInvalid
        REGEXP_ERROR_PREFIX "invalid group specifier name",                         // InvalidGroupName
        REGEXP_ERROR_PREFIX "duplicate group specifier name",                       // DuplicateGroupName
        REGEXP_ERROR_PREFIX "missing terminating ] for character class",            // CharacterClassUnmatched
        REGEXP_ERROR_PREFIX "range out of order in character class",                // CharacterClassRangeOutOfOrder
        REGEXP_ERROR_PREFIX "invalid range in character class for Unicode pattern", // CharacterClassRangeInvalid
        REGEXP_ERROR_PREFIX "\\ at end of pattern",                                 // EscapeUnterminated
        REGEXP_ERROR_PREFIX "invalid Unicode \\u escape",                           // InvalidUnicodeEscape
        REGEXP_ERROR_PREFIX "invalid Unicode code point \\u{} escape",              // InvalidUnicodeCodePointEscape
        REGEXP_ERROR_PREFIX "invalid backreference for Unicode pattern",            // InvalidBackreference
        REGEXP_ERROR_PREFIX "invalid \\k<> named backreference",                    // InvalidNamedBackReference
        REGEXP_ERROR_PREFIX "invalid escaped character for Unicode pattern",        // InvalidIdentityEscape
        REGEXP_ERROR_PREFIX "invalid octal escape for Unicode pattern",             // InvalidOctalEscape
        REGEXP_ERROR_PREFIX "invalid \\c escape for Unicode pattern",               // InvalidControlLetterEscape
        REGEXP_ERROR_PREFIX "invalid property expression",                          // InvalidUnicodePropertyExpression
        REGEXP_ERROR_PREFIX "too many nested disjunctions",                         // TooManyDisjunctions
        REGEXP_ERROR_PREFIX "pattern exceeds string length limits",                 // OffsetTooLarge
        REGEXP_ERROR_PREFIX "invalid flags"                                         // InvalidRegularExpressionFlags
    };

    return errorMessages[static_cast<unsigned>(error)];
}

JSObject* errorToThrow(JSGlobalObject* globalObject, ErrorCode error)
{
    switch (error) {
    case ErrorCode::NoError:
        ASSERT_NOT_REACHED();
        return nullptr;
    case ErrorCode::PatternTooLarge:
    case ErrorCode::QuantifierOutOfOrder:
    case ErrorCode::QuantifierWithoutAtom:
    case ErrorCode::QuantifierTooLarge:
    case ErrorCode::QuantifierIncomplete:
    case ErrorCode::MissingParentheses:
    case ErrorCode::BracketUnmatched:
    case ErrorCode::ParenthesesUnmatched:
    case ErrorCode::ParenthesesTypeInvalid:
    case ErrorCode::InvalidGroupName:
    case ErrorCode::DuplicateGroupName:
    case ErrorCode::CharacterClassUnmatched:
    case ErrorCode::CharacterClassRangeOutOfOrder:
    case ErrorCode::CharacterClassRangeInvalid:
    case ErrorCode::EscapeUnterminated:
    case ErrorCode::InvalidUnicodeEscape:
    case ErrorCode::InvalidUnicodeCodePointEscape:
    case ErrorCode::InvalidBackreference:
    case ErrorCode::InvalidNamedBackReference:
    case ErrorCode::InvalidIdentityEscape:
    case ErrorCode::InvalidOctalEscape:
    case ErrorCode::InvalidControlLetterEscape:
    case ErrorCode::InvalidUnicodePropertyExpression:
    case ErrorCode::OffsetTooLarge:
    case ErrorCode::InvalidRegularExpressionFlags:
        return createSyntaxError(globalObject, errorMessage(error));
    case ErrorCode::TooManyDisjunctions:
        return createOutOfMemoryError(globalObject, errorMessage(error));
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

} } // namespace JSC::Yarr
