/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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
#include "CatchScope.h"
#include "IndirectEvalExecutable.h"
#include "Interpreter.h"
#include "IntlDateTimeFormat.h"
#include "JSCInlines.h"
#include "JSInternalPromise.h"
#include "JSModuleLoader.h"
#include "JSPromise.h"
#include "Lexer.h"
#include "LiteralParser.h"
#include "ObjectConstructor.h"
#include "ParseInt.h"
#include <stdio.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/HexNumber.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

const ASCIILiteral ObjectProtoCalledOnNullOrUndefinedError { "Object.prototype.__proto__ called on null or undefined"_s };

template<unsigned charactersCount>
static Bitmap<256> makeCharacterBitmap(const char (&characters)[charactersCount])
{
    static_assert(charactersCount > 0, "Since string literal is null terminated, characterCount is always larger than 0");
    Bitmap<256> bitmap;
    for (unsigned i = 0; i < charactersCount - 1; ++i)
        bitmap.set(characters[i]);
    return bitmap;
}

template<typename CharacterType>
static JSValue encode(JSGlobalObject* globalObject, const Bitmap<256>& doNotEscape, const CharacterType* characters, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 18.2.6.1.1 Runtime Semantics: Encode ( string, unescapedSet )
    // https://tc39.github.io/ecma262/#sec-encode

    auto throwException = [&scope, globalObject] {
        return JSC::throwException(globalObject, scope, createURIError(globalObject, "String contained an illegal UTF-16 sequence."_s));
    };

    StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
    builder.reserveCapacity(length);

    // 4. Repeat
    auto* end = characters + length;
    for (auto* cursor = characters; cursor != end; ++cursor) {
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
        UChar32 codePoint;
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

static JSValue encode(JSGlobalObject* globalObject, JSValue argument, const Bitmap<256>& doNotEscape)
{
    return toStringView(globalObject, argument, [&] (StringView view) {
        if (view.is8Bit())
            return encode(globalObject, doNotEscape, view.characters8(), view.length());
        return encode(globalObject, doNotEscape, view.characters16(), view.length());
    });
}

template <typename CharType>
ALWAYS_INLINE
static JSValue decode(JSGlobalObject* globalObject, const CharType* characters, int length, const Bitmap<256>& doNotUnescape, bool strict)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
    int k = 0;
    UChar u = 0;
    while (k < length) {
        const CharType* p = characters + k;
        CharType c = *p;
        if (c == '%') {
            int charLen = 0;
            if (k <= length - 3 && isASCIIHexDigit(p[1]) && isASCIIHexDigit(p[2])) {
                const char b0 = Lexer<CharType>::convertHex(p[1], p[2]);
                const int sequenceLen = 1 + U8_COUNT_TRAIL_BYTES(b0);
                if (k <= length - sequenceLen * 3) {
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
                        UChar32 character;
                        int32_t offset = 0;
                        U8_NEXT(sequence, offset, sequenceLen, character);
                        if (character < 0)
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
                if (k <= length - 6 && p[1] == 'u'
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
        k++;
        builder.append(c);
    }
    if (UNLIKELY(builder.hasOverflowed()))
        return throwOutOfMemoryError(globalObject, scope);
    RELEASE_AND_RETURN(scope, jsString(vm, builder.toString()));
}

static JSValue decode(JSGlobalObject* globalObject, JSValue argument, const Bitmap<256>& doNotUnescape, bool strict)
{
    return toStringView(globalObject, argument, [&] (StringView view) {
        if (view.is8Bit())
            return decode(globalObject, view.characters8(), view.length(), doNotUnescape, strict);
        return decode(globalObject, view.characters16(), view.length(), doNotUnescape, strict);
    });
}

static const int SizeOfInfinity = 8;

template <typename CharType>
static bool isInfinity(const CharType* data, const CharType* end)
{
    return (end - data) >= SizeOfInfinity
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
static double jsBinaryIntegerLiteral(const CharType*& data, const CharType* end)
{
    // Binary number.
    data += 2;
    const CharType* firstDigitPosition = data;
    double number = 0;
    while (true) {
        number = number * 2 + (*data - '0');
        ++data;
        if (data == end)
            break;
        if (!isASCIIBinaryDigit(*data))
            break;
    }
    if (number >= mantissaOverflowLowerBound)
        number = parseIntOverflow(firstDigitPosition, data - firstDigitPosition, 2);

    return number;
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsOctalIntegerLiteral(const CharType*& data, const CharType* end)
{
    // Octal number.
    data += 2;
    const CharType* firstDigitPosition = data;
    double number = 0;
    while (true) {
        number = number * 8 + (*data - '0');
        ++data;
        if (data == end)
            break;
        if (!isASCIIOctalDigit(*data))
            break;
    }
    if (number >= mantissaOverflowLowerBound)
        number = parseIntOverflow(firstDigitPosition, data - firstDigitPosition, 8);

    return number;
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsHexIntegerLiteral(const CharType*& data, const CharType* end)
{
    // Hex number.
    data += 2;
    const CharType* firstDigitPosition = data;
    double number = 0;
    while (true) {
        number = number * 16 + toASCIIHexValue(*data);
        ++data;
        if (data == end)
            break;
        if (!isASCIIHexDigit(*data))
            break;
    }
    if (number >= mantissaOverflowLowerBound)
        number = parseIntOverflow(firstDigitPosition, data - firstDigitPosition, 16);

    return number;
}

// See ecma-262 6th 11.8.3
template <typename CharType>
static double jsStrDecimalLiteral(const CharType*& data, const CharType* end)
{
    RELEASE_ASSERT(data < end);

    size_t parsedLength;
    double number = parseDouble(data, end - data, parsedLength);
    if (parsedLength) {
        data += parsedLength;
        return number;
    }

    // Check for [+-]?Infinity
    switch (*data) {
    case 'I':
        if (isInfinity(data, end)) {
            data += SizeOfInfinity;
            return std::numeric_limits<double>::infinity();
        }
        break;

    case '+':
        if (isInfinity(data + 1, end)) {
            data += SizeOfInfinity + 1;
            return std::numeric_limits<double>::infinity();
        }
        break;

    case '-':
        if (isInfinity(data + 1, end)) {
            data += SizeOfInfinity + 1;
            return -std::numeric_limits<double>::infinity();
        }
        break;
    }

    // Not a number.
    return PNaN;
}

template <typename CharType>
static double toDouble(const CharType* characters, unsigned size)
{
    const CharType* endCharacters = characters + size;

    // Skip leading white space.
    for (; characters < endCharacters; ++characters) {
        if (!isStrWhiteSpace(*characters))
            break;
    }

    // Empty string.
    if (characters == endCharacters)
        return 0.0;

    double number;
    if (characters[0] == '0' && characters + 2 < endCharacters) {
        if ((characters[1] | 0x20) == 'x' && isASCIIHexDigit(characters[2]))
            number = jsHexIntegerLiteral(characters, endCharacters);
        else if ((characters[1] | 0x20) == 'o' && isASCIIOctalDigit(characters[2]))
            number = jsOctalIntegerLiteral(characters, endCharacters);
        else if ((characters[1] | 0x20) == 'b' && isASCIIBinaryDigit(characters[2]))
            number = jsBinaryIntegerLiteral(characters, endCharacters);
        else
            number = jsStrDecimalLiteral(characters, endCharacters);
    } else
        number = jsStrDecimalLiteral(characters, endCharacters);

    // Allow trailing white space.
    for (; characters < endCharacters; ++characters) {
        if (!isStrWhiteSpace(*characters))
            break;
    }
    if (characters != endCharacters)
        return PNaN;

    return number;
}

// See ecma-262 6th 11.8.3
double jsToNumber(StringView s)
{
    unsigned size = s.length();

    if (size == 1) {
        UChar c = s[0];
        if (isASCIIDigit(c))
            return c - '0';
        if (isStrWhiteSpace(c))
            return 0;
        return PNaN;
    }

    if (s.is8Bit())
        return toDouble(s.characters8(), size);
    return toDouble(s.characters16(), size);
}

static double parseFloat(StringView s)
{
    unsigned size = s.length();

    if (size == 1) {
        UChar c = s[0];
        if (isASCIIDigit(c))
            return c - '0';
        return PNaN;
    }

    if (s.is8Bit()) {
        const LChar* data = s.characters8();
        const LChar* end = data + size;

        // Skip leading white space.
        for (; data < end; ++data) {
            if (!isStrWhiteSpace(*data))
                break;
        }

        // Empty string.
        if (data == end)
            return PNaN;

        return jsStrDecimalLiteral(data, end);
    }

    const UChar* data = s.characters16();
    const UChar* end = data + size;

    // Skip leading white space.
    for (; data < end; ++data) {
        if (!isStrWhiteSpace(*data))
            break;
    }

    // Empty string.
    if (data == end)
        return PNaN;

    return jsStrDecimalLiteral(data, end);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEval, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue x = callFrame->argument(0);
    if (!x.isString())
        return JSValue::encode(x);

    if (!globalObject->evalEnabled()) {
        throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
        return JSValue::encode(jsUndefined());
    }

    String s = asString(x)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue parsedObject;
    if (s.is8Bit()) {
        LiteralParser<LChar> preparser(globalObject, s.characters8(), s.length(), NonStrictJSON, nullptr);
        parsedObject = preparser.tryLiteralParse();
    } else {
        LiteralParser<UChar> preparser(globalObject, s.characters16(), s.length(), NonStrictJSON, nullptr);
        parsedObject = preparser.tryLiteralParse();
    }
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (parsedObject)
        return JSValue::encode(parsedObject);

    SourceOrigin sourceOrigin = callFrame->callerSourceOrigin(vm);
    EvalExecutable* eval = IndirectEvalExecutable::create(globalObject, makeSource(s, sourceOrigin), DerivedContextType::None, false, EvalContextType::None);
    EXCEPTION_ASSERT(!!scope.exception() == !eval);
    if (!eval)
        return encodedJSValue();

    RELEASE_AND_RETURN(scope, JSValue::encode(vm.interpreter->execute(eval, globalObject, globalObject->globalThis(), globalObject->globalScope())));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncParseInt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue value = callFrame->argument(0);
    JSValue radixValue = callFrame->argument(1);

    // Optimized handling for numbers:
    // If the argument is 0 or a number in range 10^-6 <= n < INT_MAX+1, then parseInt
    // results in a truncation to integer. In the case of -0, this is converted to 0.
    //
    // This is also a truncation for values in the range INT_MAX+1 <= n < 10^21,
    // however these values cannot be trivially truncated to int since 10^21 exceeds
    // even the int64_t range. Negative numbers are a little trickier, the case for
    // values in the range -10^21 < n <= -1 are similar to those for integer, but
    // values in the range -1 < n <= -10^-6 need to truncate to -0, not 0.
    static const double tenToTheMinus6 = 0.000001;
    static const double intMaxPlusOne = 2147483648.0;
    if (value.isNumber()) {
        double n = value.asNumber();
        if (((n < intMaxPlusOne && n >= tenToTheMinus6) || !n) && radixValue.isUndefinedOrNull())
            return JSValue::encode(jsNumber(static_cast<int32_t>(n)));
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

    auto* jsString = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto viewWithString = jsString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(jsNumber(parseFloat(viewWithString.view)));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncDecodeURI, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static Bitmap<256> doNotUnescapeWhenDecodingURI = makeCharacterBitmap(
        "#$&+,/:;=?@"
    );

    return JSValue::encode(decode(globalObject, callFrame->argument(0), doNotUnescapeWhenDecodingURI, true));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncDecodeURIComponent, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static Bitmap<256> emptyBitmap;
    return JSValue::encode(decode(globalObject, callFrame->argument(0), emptyBitmap, true));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEncodeURI, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static Bitmap<256> doNotEscapeWhenEncodingURI = makeCharacterBitmap(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!#$&'()*+,-./:;=?@_~"
    );

    return JSValue::encode(encode(globalObject, callFrame->argument(0), doNotEscapeWhenEncodingURI));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEncodeURIComponent, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static Bitmap<256> doNotEscapeWhenEncodingURIComponent = makeCharacterBitmap(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!'()*-._~"
    );

    return JSValue::encode(encode(globalObject, callFrame->argument(0), doNotEscapeWhenEncodingURIComponent));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncEscape, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    static Bitmap<256> doNotEscape = makeCharacterBitmap(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "*+-./@_"
    );

    return JSValue::encode(toStringView(globalObject, callFrame->argument(0), [&] (StringView view) {
        VM& vm = globalObject->vm();
        StringBuilder builder;
        if (view.is8Bit()) {
            const LChar* c = view.characters8();
            for (unsigned k = 0; k < view.length(); k++, c++) {
                int u = c[0];
                if (doNotEscape.get(static_cast<LChar>(u)))
                    builder.append(*c);
                else {
                    builder.append('%');
                    builder.append(hex(u, 2));
                }
            }
            return jsString(vm, builder.toString());
        }

        const UChar* c = view.characters16();
        for (unsigned k = 0; k < view.length(); k++, c++) {
            UChar u = c[0];
            if (u >= doNotEscape.size()) {
                builder.appendLiteral("%u");
                builder.append(hex(static_cast<unsigned char>(u >> 8), 2));
                builder.append(hex(static_cast<unsigned char>(u & 0xFF), 2));
            } else if (doNotEscape.get(static_cast<LChar>(u)))
                builder.append(*c);
            else {
                builder.append('%');
                builder.append(hex(static_cast<unsigned char>(u), 2));
            }
        }

        return jsString(vm, builder.toString());
    }));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncUnescape, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(toStringView(globalObject, callFrame->argument(0), [&] (StringView view) {
        // We use int for k and length intentionally since we would like to evaluate
        // the condition `k <= length -6` even if length is less than 6.
        int k = 0;
        int length = view.length();

        StringBuilder builder;
        builder.reserveCapacity(length);

        if (view.is8Bit()) {
            const LChar* characters = view.characters8();
            LChar convertedLChar;
            while (k < length) {
                const LChar* c = characters + k;
                if (c[0] == '%' && k <= length - 6 && c[1] == 'u') {
                    if (isASCIIHexDigit(c[2]) && isASCIIHexDigit(c[3]) && isASCIIHexDigit(c[4]) && isASCIIHexDigit(c[5])) {
                        builder.append(Lexer<UChar>::convertUnicode(c[2], c[3], c[4], c[5]));
                        k += 6;
                        continue;
                    }
                } else if (c[0] == '%' && k <= length - 3 && isASCIIHexDigit(c[1]) && isASCIIHexDigit(c[2])) {
                    convertedLChar = LChar(Lexer<LChar>::convertHex(c[1], c[2]));
                    c = &convertedLChar;
                    k += 2;
                }
                builder.append(*c);
                k++;
            }
        } else {
            const UChar* characters = view.characters16();

            while (k < length) {
                const UChar* c = characters + k;
                UChar convertedUChar;
                if (c[0] == '%' && k <= length - 6 && c[1] == 'u') {
                    if (isASCIIHexDigit(c[2]) && isASCIIHexDigit(c[3]) && isASCIIHexDigit(c[4]) && isASCIIHexDigit(c[5])) {
                        convertedUChar = Lexer<UChar>::convertUnicode(c[2], c[3], c[4], c[5]);
                        c = &convertedUChar;
                        k += 5;
                    }
                } else if (c[0] == '%' && k <= length - 3 && isASCIIHexDigit(c[1]) && isASCIIHexDigit(c[2])) {
                    convertedUChar = UChar(Lexer<UChar>::convertHex(c[1], c[2]));
                    c = &convertedUChar;
                    k += 2;
                }
                k++;
                builder.append(*c);
            }
        }

        return jsString(globalObject->vm(), builder.toString());
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
    return throwVMTypeError(globalObject, scope, "'arguments', 'callee', and 'caller' cannot be accessed in this context.");
}

JSC_DEFINE_HOST_FUNCTION(globalFuncMakeTypeError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    Structure* errorStructure = globalObject->errorStructure(ErrorType::TypeError);
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, callFrame->argument(0), nullptr, TypeNothing, false));
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

    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, thisValue);

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
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = callFrame->uncheckedArgument(0);
    if (value.isObject() || value.isNull()) {
        JSObject* object = asObject(callFrame->thisValue());
        object->setPrototypeDirect(vm, value);
    }

    scope.assertNoException();
    return { };
}

JSC_DEFINE_HOST_FUNCTION(globalFuncHostPromiseRejectionTracker, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = jsCast<JSPromise*>(callFrame->argument(0));

    // InternalPromises should not be exposed to user scripts.
    if (jsDynamicCast<JSInternalPromise*>(vm, promise))
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

JSC_DEFINE_HOST_FUNCTION(globalFuncImportModule, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto reject = [&] (JSValue rejectionReason) {
        catchScope.clearException();
        promise->reject(globalObject, rejectionReason);
        catchScope.clearException();
        return JSValue::encode(promise);
    };

    auto sourceOrigin = callFrame->callerSourceOrigin(vm);
    RELEASE_ASSERT(callFrame->argumentCount() == 1);
    auto* specifier = callFrame->uncheckedArgument(0).toString(globalObject);
    if (Exception* exception = catchScope.exception())
        return reject(exception->value());

    // We always specify parameters as undefined. Once dynamic import() starts accepting fetching parameters,
    // we should retrieve this from the arguments.
    JSValue parameters = jsUndefined();
    auto* internalPromise = globalObject->moduleLoader()->importModule(globalObject, specifier, parameters, sourceOrigin);
    if (Exception* exception = catchScope.exception())
        return reject(exception->value());
    promise->resolve(globalObject, internalPromise);

    catchScope.clearException();
    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(globalFuncPropertyIsEnumerable, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(callFrame->argumentCount() == 2);
    JSObject* object = jsCast<JSObject*>(callFrame->uncheckedArgument(0));
    auto propertyName = callFrame->uncheckedArgument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    scope.release();
    PropertyDescriptor descriptor;
    bool enumerable = object->getOwnPropertyDescriptor(globalObject, propertyName, descriptor) && descriptor.enumerable();
    return JSValue::encode(jsBoolean(enumerable));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncOwnKeys, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Include, WTF::nullopt)));
}

JSC_DEFINE_HOST_FUNCTION(globalFuncDateTimeFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    IntlDateTimeFormat* dateTimeFormat = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
    dateTimeFormat->initializeDateTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    double value = callFrame->argument(2).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->format(globalObject, value)));
}

} // namespace JSC
