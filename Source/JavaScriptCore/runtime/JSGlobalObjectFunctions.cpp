/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSGlobalObjectFunctions.h"

#include "CallFrame.h"
#include "GlobalObjectMethodTable.h"
#include "ImportMap.h"
#include "IndirectEvalExecutable.h"
#include "InlineCallFrame.h"
#include "Interpreter.h"
#include "IntlDateTimeFormat.h"
#include "JSCInlines.h"
#include "JSInternalPromise.h"
#include "JSModuleLoader.h"
#include "JSPromise.h"
#include "JSSet.h"
#include "Lexer.h"
#include "LiteralParser.h"
#include "ObjectConstructorInlines.h"
#include "ParseInt.h"
#include <stdio.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

const ASCIILiteral ObjectProtoCalledOnNullOrUndefinedError { "Object.prototype.__proto__ called on null or undefined"_s };
const ASCIILiteral RestrictedPropertyAccessError { "'arguments', 'callee', and 'caller' cannot be accessed in this context."_s };

template<unsigned charactersCount>
static constexpr WTF::BitSet<256> makeCharacterBitmap(const char (&characters)[charactersCount])
{
    static_assert(charactersCount > 0, "Since string literal is null terminated, characterCount is always larger than 0");
    WTF::BitSet<256> bitmap;
    for (unsigned i = 0; i < charactersCount - 1; ++i)
        bitmap.set(characters[i]);
    return bitmap;
}

template<typename CharacterType>
static JSValue encode(JSGlobalObject* globalObject, const WTF::BitSet<256>& doNotEscape, std::span<const CharacterType> characters)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 18.2.6.1.1 Runtime Semantics: Encode ( string, unescapedSet )
    // https://tc39.github.io/ecma262/#sec-encode

    auto throwException = [&scope, globalObject] {
        return JSC::throwException(globalObject, scope, createURIError(globalObject, "String contained an illegal UTF-16 sequence."_s));
    };

    StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
    builder.reserveCapacity(characters.size());

    // 4. Repeat
    auto* end = characters.data() + characters.size();
    for (auto* cursor = characters.data(); cursor != end; ++cursor) {
        auto character = *cursor;

        // 4-c. If C is in unescapedSet, then
        if (character < doNotEscape.size() && doNotEscape.get(character)) {
            // 4-c-i. Let S be a String containing only the code unit C.
            // 4-c-ii. Let R be a new String value computed by concatenating the previous value of R and S.
            builder.append(static_cast<LChar>(character));
            continue;
        }

        // 4-d-i. If the code unit value of C is not less than 0xDC00 and not greater than 0xDFFF, throw a URIError exception.
        if (U16_IS_TRAIL(character))
            return throwException();

        // 4-d-ii. If the code unit value of C is less than 0xD800 or greater than 0xDBFF, then
        // 4-d-ii-1. Let V be the code unit value of C.
        char32_t codePoint;
        if (!U16_IS_LEAD(character))
            codePoint = character;
        else {
            // 4-d-iii. Else,
            // 4-d-iii-1. Increase k by 1.
            ++cursor;

            // 4-d-iii-2. If k equals strLen, throw a URIError exception.
            if (cursor == end)
                return throwException();

            // 4-d-iii-3. Let kChar be the code unit value of the code unit at index k within string.
            auto trail = *cursor;

            // 4-d-iii-4. If kChar is less than 0xDC00 or greater than 0xDFFF, throw a URIError exception.
            if (!U16_IS_TRAIL(trail))
                return throwException();

            // 4-d-iii-5. Let V be UTF16Decode(C, kChar).
            codePoint = U16_GET_SUPPLEMENTARY(character, trail);
        }

        // 4-d-iv. Let Octets be the array of octets resulting by applying the UTF-8 transformation to V, and let L be the array size.
        LChar utf8OctetsBuffer[U8_MAX_LENGTH];
        unsigned utf8Length = 0;
        // We can use U8_APPEND_UNSAFE here since codePoint is either
        // 1. non surrogate one, correct code point.
        // 2. correct code point generated from validated lead and trail surrogates.
        U8_APPEND_UNSAFE(utf8OctetsBuffer, utf8Length, codePoint);

        // 4-d-v. Let j be 0.
        // 4-d-vi. Repeat, while j < L
        for (unsigned index = 0; index < utf8Length; ++index) {
            // 4-d-vi-1. Let jOctet be the value at index j within Octets.
            // 4-d-vi-2. Let S be a String containing three code units "%XY" where XY are two uppercase hexadecimal digits encoding the value of jOctet.
            // 4-d-vi-3. Let R be a new String value computed by concatenating the previous value of R and S.
            builder.append('%');
            builder.append(hex(utf8OctetsBuffer[index], 2));
        }
    }

    if (UNLIKELY(builder.hasOverflowed()))
        return throwOutOfMemoryError(globalObject, scope);
    return jsString(vm, builder.toString());
}

static JSValue encode(JSGlobalObject* globalObject, JSValue argument, const WTF::BitSet<256>& doNotEscape)
{
    return toStringView(globalObject, argument, [&] (StringView view) {
        if (view.is8Bit())
            return encode(globalObject, doNotEscape, view.span8());
        return encode(globalObject, doNotEscape, view.span16());
    });
}

template <typename CharType>
ALWAYS_INLINE
static JSValue decode(JSGlobalObject* globalObject, std::span<const CharType> characters, const WTF::BitSet<256>& doNotUnescape, bool strict)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
    size_t k = 0;
    UChar u = 0;
    while (k < characters.size()) {
        const CharType* p = characters.data() + k;
        CharType c = *p;
        if (c == '%') {
            size_t charLen = 0;
            if (k + 3 <= characters.size() && isASCIIHexDigit(p[1]) && isASCIIHexDigit(p[2])) {
                const char b0 = Lexer<CharType>::convertHex(p[1], p[2]);
                const int sequenceLen = 1 + U8_COUNT_TRAIL_BYTES(b0);
                if ((k + sequenceLen * 3) <= characters.size()) {
                    charLen = sequenceLen * 3;
                    uint8_t sequence[U8_MAX_LENGTH];
                    sequence[0] = b0;
                    for (int i = 1; i < sequenceLen; ++i) {
                        const CharType* q = p + i * 3;
                        if (q[0] == '%' && isASCIIHexDigit(q[1]) && isASCIIHexDigit(q[2]))
                            sequence[i] = Lexer<CharType>::convertHex(q[1], q[2]);
                        else {
                            charLen = 0;
                            break;
                        }
                    }
                    if (charLen != 0) {
                        char32_t character;
                        int32_t offset = 0;
                        U8_NEXT(sequence, offset, sequenceLen, character);
                        if (character == static_cast<char32_t>(U_SENTINEL))
                            charLen = 0;
                        else if (!U_IS_BMP(character)) {
                            // Convert to surrogate pair.
                            ASSERT(U_IS_SUPPLEMENTARY(character));
                            builder.append(U16_LEAD(character));
                            u = U16_TRAIL(character);
                        } else {
                            ASSERT(!U_IS_SURROGATE(character));
                            u = static_cast<UChar>(character);
                        }
                    }
                }
            }
            if (charLen == 0) {
                if (strict)
                    return throwException(globalObject, scope, createURIError(globalObject, "URI error"_s));
                // The only case where we don't use "strict" mode is the "unescape" function.
                // For that, it's good to support the wonky "%u" syntax for compatibility with WinIE.
                if (k + 6 <= characters.size() && p[1] == 'u'
                        && isASCIIHexDigit(p[2]) && isASCIIHexDigit(p[3])
                        && isASCIIHexDigit(p[4]) && isASCIIHexDigit(p[5])) {
                    charLen = 6;
                    u = Lexer<UChar>::convertUnicode(p[2], p[3], p[4], p[5]);
                }
            }
            if (charLen && (u >= 128 || !doNotUnescape.get(static_cast<LChar>(u)))) {
                builder.append(u);
                k += charLen;
                continue;
            }
        }
        ++k;
        builder.append(c);
    }
    if (UNLIKELY(builder.hasOverflowed()))
        return throwOutOfMemoryError(globalObject, scope);
    RELEASE_AND_RETURN(scope, jsString(vm, builder.toString()));
}

static JSValue decode(JSGlobalObject* globalObject, JSValue argument, const WTF::BitSet<256>& doNotUnescape, bool strict)
{
    return toStringView(globalObject, argument, [&] (StringView view) {
        if (view.is8Bit())
            return decode(globalObject, view.span8(), doNotUnescape, strict);
        return decode(globalObject, view.span16(), doNotUnescape, strict);
    });
}

static const int SizeOfInfinity = 8;

template <typename CharType>
static bool isInfinity(std::span<const CharType> data)
{
    return data.size() >= SizeOfInfinity
        && data[0] == 'I'
        && data[1] == 'n'
        && data[2] == 'f'
        && data[3] == 'i'
        && data[4] == 'n'
        && data[5] == 'i'
        && data[6] == 't'
        && data[7] == 'y';
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsBinaryIntegerLiteral(std::span<const CharType>& data)
{
    // Binary number.
    data = data.subspan(2);
    auto firstDigitPosition = data;
    double number = 0;
    while (true) {
        number = number * 2 + (data.front() - '0');
        data = data.subspan(1);
        if (data.empty())
            break;
        if (!isASCIIBinaryDigit(data.front()))
            break;
    }
    if (number >= mantissaOverflowLowerBound)
        number = parseIntOverflow(firstDigitPosition.first(data.data() - firstDigitPosition.data()), 2);

    return number;
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsOctalIntegerLiteral(std::span<const CharType>& data)
{
    // Octal number.
    data = data.subspan(2);
    auto firstDigitPosition = data;
    double number = 0;
    while (true) {
        number = number * 8 + (data.front() - '0');
        data = data.subspan(1);
        if (data.empty())
            break;
        if (!isASCIIOctalDigit(data.front()))
            break;
    }
    if (number >= mantissaOverflowLowerBound)
        number = parseIntOverflow(firstDigitPosition.first(data.data() - firstDigitPosition.data()), 8);

    return number;
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsHexIntegerLiteral(std::span<const CharType>& data)
{
    // Hex number.
    data = data.subspan(2);
    auto firstDigitPosition = data;
    double number = 0;
    while (true) {
        number = number * 16 + toASCIIHexValue(data.front());
        data = data.subspan(1);
        if (data.empty())
            break;
        if (!isASCIIHexDigit(data.front()))
            break;
    }
    if (number >= mantissaOverflowLowerBound)
        number = parseIntOverflow(firstDigitPosition.first(data.data() - firstDigitPosition.data()), 16);

    return number;
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsStrDecimalLiteral(std::span<const CharType>& data)
{
    RELEASE_ASSERT(!data.empty());

    size_t parsedLength;
    double number = parseDouble(data, parsedLength);
    if (parsedLength) {
        data = data.subspan(parsedLength);
        return number;
    }

    // Check for [+-]?Infinity
    switch (data.front()) {
    case 'I':
        if (isInfinity(data)) {
            data = data.subspan(SizeOfInfinity);
            return std::numeric_limits<double>::infinity();
        }
        break;

    case '+':
        if (isInfinity(data.subspan(1))) {
            data = data.subspan(SizeOfInfinity + 1);
            return std::numeric_limits<double>::infinity();
        }
        break;

    case '-':
        if (isInfinity(data.subspan(1))) {
            data = data.subspan(SizeOfInfinity + 1);
            return -std::numeric_limits<double>::infinity();
        }
        break;
    }

    // Not a number.
    return PNaN;
}

template <typename CharacterType>
static double toDouble(std::span<const CharacterType> characters)
{
    // Skip leading white space.
    for (; !characters.empty(); characters = characters.subspan(1)) {
        if (!isStrWhiteSpace(characters.front()))
            break;
    }

    // Empty string.
    if (characters.empty())
        return 0.0;

    double number;
    if (characters.front() == '0' && characters.size() > 2) {
        if ((characters[1] | 0x20) == 'x' && isASCIIHexDigit(characters[2]))
            number = jsHexIntegerLiteral(characters);
        else if ((characters[1] | 0x20) == 'o' && isASCIIOctalDigit(characters[2]))
            number = jsOctalIntegerLiteral(characters);
        else if ((characters[1] | 0x20) == 'b' && isASCIIBinaryDigit(characters[2]))
            number = jsBinaryIntegerLiteral(characters);
        else
            number = jsStrDecimalLiteral(characters);
    } else
        number = jsStrDecimalLiteral(characters);

    // Allow trailing white space.
    for (; !characters.empty(); characters = characters.subspan(1)) {
        if (!isStrWhiteSpace(characters.front()))
            break;
    }
    if (!characters.empty())
        return PNaN;

    return number;
}

// See ecma-262 6th 11.8.3
template<typename CharacterType>
static ALWAYS_INLINE double jsToNumber(std::span<const CharacterType> characters)
{
    if (characters.size() == 1) {
        auto c = characters.front();
        if (isASCIIDigit(c))
            return c - '0';
        if (isStrWhiteSpace(c))
            return 0;
        return PNaN;
    }

    if (characters.size() == 2 && characters.front() == '-') {
        auto c = characters[1];
        if (c == '0')
            return -0.0;
        if (isASCIIDigit(c))
            return -static_cast<int32_t>(c - '0');
        return PNaN;
    }

    return toDouble(characters);
}

double jsToNumber(StringView s)
{
    if (s.is8Bit())
        return jsToNumber(s.span8());
    return jsToNumber(s.span16());
}

static double parseFloat(StringView s)
{
    if (s.length() == 1) {
        UChar c = s[0];
        if (isASCIIDigit(c))
            return c - '0';
        return PNaN;
    }

    if (s.is8Bit()) {
        auto data = s.span8();

        // Skip leading white space.
        for (; !data.empty(); data = data.subspan(1)) {
            if (!isStrWhiteSpace(data.front()))
                break;
        }

        // Empty string.
        if (data.empty())
            return PNaN;

        return jsStrDecimalLiteral(data);
    }

    auto data = s.span16();

    // Skip leading white space.
    for (; !data.empty(); data = data.subspan(1)) {
        if (!isStrWhiteSpace(data.front()))
            break;
    }

    // Empty string.
    if (data.empty())
        return PNaN;

    return jsStrDecimalLiteral(data);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEval, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue x = callFrame->argument(0);
    JSString* codeString = nullptr;
    if (LIKELY(x.isString()))
        codeString = asString(x);
    else if (Options::useTrustedTypes()) {
        auto code = globalObject->globalObjectMethodTable()->codeForEval(globalObject, x);
        if (!code.isNull())
            codeString = jsString(vm, code);
    }

    if (!codeString)
        return JSValue::encode(x);

    auto s = codeString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (Options::useTrustedTypes() && globalObject->requiresTrustedTypes() && !globalObject->globalObjectMethodTable()->canCompileStrings(globalObject, CompilationType::IndirectEval, s, x)) {
        throwException(globalObject, scope, createEvalError(globalObject, "Refused to evaluate a string as JavaScript because this document requires a 'Trusted Type' assignment."_s));
        return { };
    }

    if (!globalObject->evalEnabled()) {
        globalObject->globalObjectMethodTable()->reportViolationForUnsafeEval(globalObject, codeString);
        throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
        return JSValue::encode(jsUndefined());
    }

    JSValue parsedObject;
    if (s->is8Bit()) {
        LiteralParser preparser(globalObject, s->span8(), SloppyJSON, nullptr);
        parsedObject = preparser.tryLiteralParse();
    } else {
        LiteralParser preparser(globalObject, s->span16(), SloppyJSON, nullptr);
        parsedObject = preparser.tryLiteralParse();
    }
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (parsedObject)
        return JSValue::encode(parsedObject);

    SourceOrigin sourceOrigin = callFrame->callerSourceOrigin(vm);
    SourceTaintedOrigin sourceTaintedOrigin = computeNewSourceTaintedOriginFromStack(vm, callFrame);
    LexicallyScopedFeatures lexicallyScopedFeatures = globalObject->globalScopeExtension() ? TaintedByWithScopeLexicallyScopedFeature : NoLexicallyScopedFeatures;
    EvalExecutable* eval = IndirectEvalExecutable::tryCreate(globalObject, makeSource(s, sourceOrigin, sourceTaintedOrigin), lexicallyScopedFeatures, DerivedContextType::None, false, EvalContextType::None);
    EXCEPTION_ASSERT(!!scope.exception() == !eval);
    if (!eval)
        return encodedJSValue();

    RELEASE_AND_RETURN(scope, JSValue::encode(vm.interpreter.executeEval(eval, globalObject->globalThis(), globalObject->globalScope())));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncParseInt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue value = callFrame->argument(0);
    JSValue radixValue = callFrame->argument(1);

    if (value.isNumber()) {
        if (radixValue.isUndefinedOrNull() || (radixValue.isInt32() && radixValue.asInt32() == 10)) {
            if (value.isInt32())
                return JSValue::encode(value);
            if (auto result = parseIntDouble(value.asDouble()))
                return JSValue::encode(jsNumber(result.value()));
        }
    }

    // If ToString throws, we shouldn't call ToInt32.
    return toStringView(globalObject, value, [&] (StringView view) {
        return JSValue::encode(jsNumber(parseInt(view, radixValue.toInt32(globalObject))));
    });
}

JSC_DEFINE_HOST_FUNCTION(globalFuncParseFloat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = callFrame->argument(0);
    if (value.isNumber()) {
        if (value.isInt32())
            return JSValue::encode(value);
        if (value.asDouble() == 0.0) // Makes -0.0 to 0.0 too.
            return JSValue::encode(jsNumber(0.0));
        return JSValue::encode(value);
    }

    auto* jsString = value.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto view = jsString->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(jsNumber(parseFloat(view)));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncDecodeURI, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static constexpr auto doNotUnescapeWhenDecodingURI = makeCharacterBitmap(
        "#$&+,/:;=?@"
    );

    return JSValue::encode(decode(globalObject, callFrame->argument(0), doNotUnescapeWhenDecodingURI, true));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncDecodeURIComponent, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static constexpr WTF::BitSet<256> emptyBitmap;
    return JSValue::encode(decode(globalObject, callFrame->argument(0), emptyBitmap, true));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEncodeURI, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static constexpr auto doNotEscapeWhenEncodingURI = makeCharacterBitmap(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!#$&'()*+,-./:;=?@_~"
    );
    return JSValue::encode(encode(globalObject, callFrame->argument(0), doNotEscapeWhenEncodingURI));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEncodeURIComponent, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static constexpr auto doNotEscapeWhenEncodingURIComponent = makeCharacterBitmap(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!'()*-._~"
    );
    return JSValue::encode(encode(globalObject, callFrame->argument(0), doNotEscapeWhenEncodingURIComponent));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEscape, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(toStringView(globalObject, callFrame->argument(0), [&] (StringView view) -> JSString* {
        static constexpr auto doNotEscape = makeCharacterBitmap(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "*+-./@_"
        );

        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
        if (view.is8Bit()) {
            for (auto character : view.span8()) {
                if (doNotEscape.get(character))
                    builder.append(character);
                else
                    builder.append('%', hex(character, 2));
            }
        } else {
            for (auto character : view.span16()) {
                if (character >= doNotEscape.size())
                    builder.append("%u"_s, hex(static_cast<uint8_t>(character >> 8), 2), hex(static_cast<uint8_t>(character), 2));
                else if (doNotEscape.get(static_cast<LChar>(character)))
                    builder.append(character);
                else
                    builder.append('%', hex(character, 2));
            }
        }

        if (UNLIKELY(builder.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        return jsString(vm, builder.toString());
    }));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncUnescape, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(toStringView(globalObject, callFrame->argument(0), [&] (StringView view) -> JSString* {
        // We use int for k and length intentionally since we would like to evaluate
        // the condition `k <= length -6` even if length is less than 6.
        int k = 0;
        int length = view.length();

        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
        builder.reserveCapacity(length);

        if (view.is8Bit()) {
            auto characters = view.span8();
            LChar convertedLChar;
            while (k < length) {
                auto c = characters.subspan(k);
                if (c[0] == '%' && k <= length - 6 && c[1] == 'u') {
                    if (isASCIIHexDigit(c[2]) && isASCIIHexDigit(c[3]) && isASCIIHexDigit(c[4]) && isASCIIHexDigit(c[5])) {
                        builder.append(Lexer<UChar>::convertUnicode(c[2], c[3], c[4], c[5]));
                        k += 6;
                        continue;
                    }
                } else if (c[0] == '%' && k <= length - 3 && isASCIIHexDigit(c[1]) && isASCIIHexDigit(c[2])) {
                    convertedLChar = LChar(Lexer<LChar>::convertHex(c[1], c[2]));
                    c = span(convertedLChar);
                    k += 2;
                }
                builder.append(c.front());
                ++k;
            }
        } else {
            auto characters = view.span16();

            while (k < length) {
                auto c = characters.subspan(k);
                UChar convertedUChar;
                if (c[0] == '%' && k <= length - 6 && c[1] == 'u') {
                    if (isASCIIHexDigit(c[2]) && isASCIIHexDigit(c[3]) && isASCIIHexDigit(c[4]) && isASCIIHexDigit(c[5])) {
                        convertedUChar = Lexer<UChar>::convertUnicode(c[2], c[3], c[4], c[5]);
                        c = span(convertedUChar);
                        k += 5;
                    }
                } else if (c[0] == '%' && k <= length - 3 && isASCIIHexDigit(c[1]) && isASCIIHexDigit(c[2])) {
                    convertedUChar = UChar(Lexer<UChar>::convertHex(c[1], c[2]));
                    c = span(convertedUChar);
                    k += 2;
                }
                ++k;
                builder.append(c.front());
            }
        }

        if (UNLIKELY(builder.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        return jsString(vm, builder.toString());
    }));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncThrowTypeError, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncThrowTypeErrorArgumentsCalleeAndCaller, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, RestrictedPropertyAccessError);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncMakeTypeError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    Structure* errorStructure = globalObject->errorStructure(ErrorType::TypeError);
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, callFrame->argument(0), jsUndefined(), nullptr, TypeNothing, ErrorType::TypeError, false));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncProtoGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    return JSValue::encode(thisValue.getPrototype(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncProtoSetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    if (thisValue.isUndefinedOrNull())
        return throwVMTypeError(globalObject, scope, ObjectProtoCalledOnNullOrUndefinedError);

    JSValue value = callFrame->argument(0);

    JSObject* thisObject = jsDynamicCast<JSObject*>(thisValue);

    // Setting __proto__ of a primitive should have no effect.
    if (!thisObject)
        return JSValue::encode(jsUndefined());

    // Setting __proto__ to a non-object, non-null value is silently ignored to match Mozilla.
    if (!value.isObject() && !value.isNull())
        return JSValue::encode(jsUndefined());

    scope.release();
    bool shouldThrowIfCantSet = true;
    thisObject->setPrototype(vm, globalObject, value, shouldThrowIfCantSet);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncSetPrototypeDirect, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    JSValue value = callFrame->uncheckedArgument(0);
    if (value.isObject() || value.isNull()) {
        JSObject* object = asObject(callFrame->thisValue());
        object->setPrototypeDirect(vm, value);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncSetPrototypeDirectOrThrow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = callFrame->uncheckedArgument(0);
    if (!value.isObject() && !value.isNull())
        return throwVMError(globalObject, scope, createInvalidPrototypeError(globalObject, value));

    JSObject* object = asObject(callFrame->thisValue());
    object->setPrototypeDirect(vm, value);

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncHostPromiseRejectionTracker, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = jsCast<JSPromise*>(callFrame->argument(0));

    // InternalPromises should not be exposed to user scripts.
    if (jsDynamicCast<JSInternalPromise*>(promise))
        return JSValue::encode(jsUndefined());

    JSValue operationValue = callFrame->argument(1);

    ASSERT(operationValue.isNumber());
    auto operation = static_cast<JSPromiseRejectionOperation>(operationValue.toUInt32(globalObject));
    ASSERT(operation == JSPromiseRejectionOperation::Reject || operation == JSPromiseRejectionOperation::Handle);
    scope.assertNoException();

    if (globalObject->globalObjectMethodTable()->promiseRejectionTracker)
        globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, operation);
    else {
        switch (operation) {
        case JSPromiseRejectionOperation::Reject:
            vm.promiseRejected(promise);
            break;
        case JSPromiseRejectionOperation::Handle:
            // do nothing
            break;
        }
    }
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncBuiltinLog, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    dataLog(callFrame->argument(0).toWTFString(globalObject), "\n");
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncBuiltinDescribe, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsString(globalObject->vm(), toString(callFrame->argument(0))));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncImportMapStatus, (JSGlobalObject* globalObject, CallFrame*))
{
    // https://wicg.github.io/import-maps/#integration-wait-for-import-maps
    globalObject->importMap().setAcquiringImportMaps();
    if (auto* promise = globalObject->importMapStatusPromise())
        return JSValue::encode(promise);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncImportModule, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto sourceOrigin = callFrame->callerSourceOrigin(vm);
    RELEASE_ASSERT(callFrame->argumentCount() >= 1);
    auto* specifier = callFrame->uncheckedArgument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    // We always specify parameters as undefined. Once dynamic import() starts accepting fetching parameters,
    // we should retrieve this from the arguments.
    JSValue parameters = callFrame->argument(1);
    auto* internalPromise = globalObject->moduleLoader()->importModule(globalObject, specifier, parameters, sourceOrigin);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    scope.release();
    promise->resolve(globalObject, internalPromise);
    return JSValue::encode(promise);
}

static bool canPerformFastPropertyEnumerationForCopyDataProperties(Structure* structure)
{
    if (structure->typeInfo().overridesGetOwnPropertySlot())
        return false;
    if (structure->typeInfo().overridesAnyFormOfGetOwnPropertyNames())
        return false;
    // FIXME: Indexed properties can be handled.
    // https://bugs.webkit.org/show_bug.cgi?id=185358
    if (hasIndexedProperties(structure->indexingType()))
        return false;
    if (structure->hasAnyKindOfGetterSetterProperties())
        return false;
    if (structure->isUncacheableDictionary())
        return false;
    return true;
};

static CodeBlock* getCallerCodeBlock(CallFrame* callFrame)
{
    CallFrame* callerFrame = callFrame->callerFrame();
    CodeOrigin codeOrigin = callerFrame->codeOrigin();
    if (codeOrigin && codeOrigin.inlineCallFrame())
        return baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame());
    if (callerFrame->isNativeCalleeFrame())
        return nullptr;
    return callerFrame->codeBlock();
}

// https://tc39.es/ecma262/#sec-copydataproperties
JSC_DEFINE_HOST_FUNCTION(globalFuncCopyDataProperties, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFinalObject* target = jsCast<JSFinalObject*>(callFrame->thisValue());
    ASSERT(target->isStructureExtensible());

    JSValue sourceValue = callFrame->uncheckedArgument(0);
    if (sourceValue.isUndefinedOrNull())
        return JSValue::encode(target);

    JSObject* source = sourceValue.toObject(globalObject);
    scope.assertNoException();

    UnlinkedCodeBlock* unlinkedCodeBlock = nullptr;
    const IdentifierSet* excludedSet = nullptr;
    std::optional<IdentifierSet> newlyCreatedSet;
    if (callFrame->argumentCount() > 1) {
        int32_t setIndex = callFrame->uncheckedArgument(1).asUInt32AsAnyInt();
        CodeBlock* codeBlock = getCallerCodeBlock(callFrame);
        ASSERT(codeBlock);
        unlinkedCodeBlock = codeBlock->unlinkedCodeBlock();
        excludedSet = &unlinkedCodeBlock->constantIdentifierSets()[setIndex];
        if (callFrame->argumentCount() > 2) {
            newlyCreatedSet.emplace(*excludedSet);
            for (unsigned index = 2; index < callFrame->argumentCount(); ++index) {
                // This isn't observable since ObjectPatternNode::bindValue() also performs ToPropertyKey.
                auto propertyName = callFrame->uncheckedArgument(index).toPropertyKey(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                newlyCreatedSet->add(propertyName.impl());
            }
            excludedSet = &newlyCreatedSet.value();
        }
    }

    auto isPropertyNameExcluded = [&] (PropertyName propertyName) -> bool {
        ASSERT(!propertyName.isPrivateName());
        if (!excludedSet)
            return false;
        return excludedSet->contains(propertyName.uid());
    };

    if (!source->staticPropertiesReified()) {
        source->reifyAllStaticProperties(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (LIKELY(canPerformFastPropertyEnumerationForCopyDataProperties(source->structure()))) {
        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;

        // FIXME: It doesn't seem like we should have to do this in two phases, but
        // we're running into crashes where it appears that source is transitioning
        // under us, and even ends up in a state where it has a null butterfly. My
        // leading hypothesis here is that we fire some value replacement watchpoint
        // that ends up transitioning the structure underneath us.
        // https://bugs.webkit.org/show_bug.cgi?id=187837

        source->structure()->forEachProperty(vm, [&](const PropertyTableEntry& entry) ALWAYS_INLINE_LAMBDA {
            PropertyName propertyName(entry.key());
            if (propertyName.isPrivateName())
                return true;

            if (entry.attributes() & PropertyAttribute::DontEnum)
                return true;

            if (isPropertyNameExcluded(propertyName))
                return true;

            properties.append(entry.key());
            values.appendWithCrashOnOverflow(source->getDirect(entry.offset()));
            return true;
        });
        RETURN_IF_EXCEPTION(scope, { });

        // excludedSet is no longer used.
        ensureStillAliveHere(unlinkedCodeBlock);

        if (LIKELY(target->inherits<JSFinalObject>() && target->canPerformFastPutInlineExcludingProto() && target->isStructureExtensible()))
            target->putOwnDataPropertyBatching(vm, properties.data(), values.data(), properties.size());
        else {
            for (size_t i = 0; i < properties.size(); ++i)
                target->putDirect(vm, properties[i].get(), values.at(i));
        }
        return JSValue::encode(target);
    }

    PropertyNameArray propertyNames(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    source->methodTable()->getOwnPropertyNames(source, globalObject, propertyNames, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, { });

    for (const auto& propertyName : propertyNames) {
        if (isPropertyNameExcluded(propertyName))
            continue;

        PropertySlot slot(source, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = source->methodTable()->getOwnPropertySlot(source, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasProperty)
            continue;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            continue;

        JSValue value;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            value = slot.getValue(globalObject, propertyName);
        else
            value = source->get(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, { });

        target->putDirectMayBeIndex(globalObject, propertyName, value);
        RETURN_IF_EXCEPTION(scope, { });
    }
    ensureStillAliveHere(unlinkedCodeBlock);
    return JSValue::encode(target);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncCloneObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue sourceValue = callFrame->thisValue();
    if (sourceValue.isUndefinedOrNull())
        RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyObject(globalObject)));

    JSObject* source = sourceValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!source->staticPropertiesReified()) {
        source->reifyAllStaticProperties(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    Structure* sourceStructure = source->structure();
    if (LIKELY(sourceStructure->canPerformFastPropertyEnumerationCommon())) {
        if (auto* cloned = tryCreateObjectViaCloning(vm, globalObject, source))
            return JSValue::encode(cloned);
    }

    JSObject* target = constructEmptyObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (LIKELY(canPerformFastPropertyEnumerationForCopyDataProperties(sourceStructure))) {
        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;

        // FIXME: It doesn't seem like we should have to do this in two phases, but
        // we're running into crashes where it appears that source is transitioning
        // under us, and even ends up in a state where it has a null butterfly. My
        // leading hypothesis here is that we fire some value replacement watchpoint
        // that ends up transitioning the structure underneath us.
        // https://bugs.webkit.org/show_bug.cgi?id=187837

        source->structure()->forEachProperty(vm, [&](const PropertyTableEntry& entry) ALWAYS_INLINE_LAMBDA {
            PropertyName propertyName(entry.key());
            if (propertyName.isPrivateName())
                return true;

            if (entry.attributes() & PropertyAttribute::DontEnum)
                return true;

            properties.append(entry.key());
            values.appendWithCrashOnOverflow(source->getDirect(entry.offset()));
            return true;
        });
        RETURN_IF_EXCEPTION(scope, { });

        target->putOwnDataPropertyBatching(vm, properties.data(), values.data(), properties.size());
        return JSValue::encode(target);
    }

    PropertyNameArray propertyNames(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    source->methodTable()->getOwnPropertyNames(source, globalObject, propertyNames, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, { });

    for (const auto& propertyName : propertyNames) {
        PropertySlot slot(source, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = source->methodTable()->getOwnPropertySlot(source, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasProperty)
            continue;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            continue;

        JSValue value;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            value = slot.getValue(globalObject, propertyName);
        else
            value = source->get(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, { });

        target->putDirectMayBeIndex(globalObject, propertyName, value);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return JSValue::encode(target);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncHandleNegativeProxyHasTrapResult, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* target = asObject(callFrame->uncheckedArgument(0));

    Identifier propertyName = callFrame->uncheckedArgument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    ProxyObject::validateNegativeHasTrapResult(globalObject, target, propertyName);

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncHandleProxyGetTrapResult, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue trapResult = callFrame->uncheckedArgument(0);
    JSObject* target = asObject(callFrame->uncheckedArgument(1));

    Identifier propertyName = callFrame->uncheckedArgument(2).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    ProxyObject::validateGetTrapResult(globalObject, trapResult, target, propertyName);

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncHandlePositiveProxySetTrapResult, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* target = asObject(callFrame->uncheckedArgument(0));

    Identifier propertyName = callFrame->uncheckedArgument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue putValue = callFrame->uncheckedArgument(2);

    scope.release();
    ProxyObject::validatePositiveSetTrapResult(globalObject, target, propertyName, putValue);

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(globalFuncIsNaN, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue argument = callFrame->argument(0);
    return JSValue::encode(jsBoolean(std::isnan(argument.toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncToIntegerOrInfinity, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue argument = callFrame->argument(0);
    if (argument.isInt32())
        return JSValue::encode(argument);
    return JSValue::encode(jsNumber(argument.toIntegerOrInfinity(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncToLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue argument = callFrame->argument(0);
    if (argument.isInt32())
        return JSValue::encode(jsNumber(std::max<int32_t>(argument.asInt32(), 0)));
    return JSValue::encode(jsNumber(argument.toLength(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncSpeciesGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
}

} // namespace JSC
