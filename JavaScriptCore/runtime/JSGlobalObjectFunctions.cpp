/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "GlobalEvalFunction.h"
#include "JSGlobalObject.h"
#include "LiteralParser.h"
#include "JSString.h"
#include "Interpreter.h"
#include "Parser.h"
#include "dtoa.h"
#include "Lexer.h"
#include "Nodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/unicode/UTF8.h>

using namespace WTF;
using namespace Unicode;

namespace JSC {

static JSValue encode(ExecState* exec, const ArgList& args, const char* doNotEscape)
{
    UString str = args.at(0).toString(exec);
    CString cstr = str.UTF8String(true);
    if (!cstr.c_str())
        return throwError(exec, URIError, "String contained an illegal UTF-16 sequence.");

    UString result = "";
    const char* p = cstr.c_str();
    for (size_t k = 0; k < cstr.size(); k++, p++) {
        char c = *p;
        if (c && strchr(doNotEscape, c))
            result.append(c);
        else {
            char tmp[4];
            sprintf(tmp, "%%%02X", static_cast<unsigned char>(c));
            result += tmp;
        }
    }
    return jsString(exec, result);
}

static JSValue decode(ExecState* exec, const ArgList& args, const char* doNotUnescape, bool strict)
{
    UString result = "";
    UString str = args.at(0).toString(exec);
    int k = 0;
    int len = str.size();
    const UChar* d = str.data();
    UChar u = 0;
    while (k < len) {
        const UChar* p = d + k;
        UChar c = *p;
        if (c == '%') {
            int charLen = 0;
            if (k <= len - 3 && isASCIIHexDigit(p[1]) && isASCIIHexDigit(p[2])) {
                const char b0 = Lexer::convertHex(p[1], p[2]);
                const int sequenceLen = UTF8SequenceLength(b0);
                if (sequenceLen != 0 && k <= len - sequenceLen * 3) {
                    charLen = sequenceLen * 3;
                    char sequence[5];
                    sequence[0] = b0;
                    for (int i = 1; i < sequenceLen; ++i) {
                        const UChar* q = p + i * 3;
                        if (q[0] == '%' && isASCIIHexDigit(q[1]) && isASCIIHexDigit(q[2]))
                            sequence[i] = Lexer::convertHex(q[1], q[2]);
                        else {
                            charLen = 0;
                            break;
                        }
                    }
                    if (charLen != 0) {
                        sequence[sequenceLen] = 0;
                        const int character = decodeUTF8Sequence(sequence);
                        if (character < 0 || character >= 0x110000)
                            charLen = 0;
                        else if (character >= 0x10000) {
                            // Convert to surrogate pair.
                            result.append(static_cast<UChar>(0xD800 | ((character - 0x10000) >> 10)));
                            u = static_cast<UChar>(0xDC00 | ((character - 0x10000) & 0x3FF));
                        } else
                            u = static_cast<UChar>(character);
                    }
                }
            }
            if (charLen == 0) {
                if (strict)
                    return throwError(exec, URIError);
                // The only case where we don't use "strict" mode is the "unescape" function.
                // For that, it's good to support the wonky "%u" syntax for compatibility with WinIE.
                if (k <= len - 6 && p[1] == 'u'
                        && isASCIIHexDigit(p[2]) && isASCIIHexDigit(p[3])
                        && isASCIIHexDigit(p[4]) && isASCIIHexDigit(p[5])) {
                    charLen = 6;
                    u = Lexer::convertUnicode(p[2], p[3], p[4], p[5]);
                }
            }
            if (charLen && (u == 0 || u >= 128 || !strchr(doNotUnescape, u))) {
                c = u;
                k += charLen - 1;
            }
        }
        k++;
        result.append(c);
    }
    return jsString(exec, result);
}

bool isStrWhiteSpace(UChar c)
{
    switch (c) {
        case 0x0009:
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0020:
        case 0x00A0:
        case 0x2028:
        case 0x2029:
            return true;
        default:
            return c > 0xff && isSeparatorSpace(c);
    }
}

static int parseDigit(unsigned short c, int radix)
{
    int digit = -1;

    if (c >= '0' && c <= '9')
        digit = c - '0';
    else if (c >= 'A' && c <= 'Z')
        digit = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        digit = c - 'a' + 10;

    if (digit >= radix)
        return -1;
    return digit;
}

double parseIntOverflow(const char* s, int length, int radix)
{
    double number = 0.0;
    double radixMultiplier = 1.0;

    for (const char* p = s + length - 1; p >= s; p--) {
        if (radixMultiplier == Inf) {
            if (*p != '0') {
                number = Inf;
                break;
            }
        } else {
            int digit = parseDigit(*p, radix);
            number += digit * radixMultiplier;
        }

        radixMultiplier *= radix;
    }

    return number;
}

static double parseInt(const UString& s, int radix)
{
    int length = s.size();
    const UChar* data = s.data();
    int p = 0;

    while (p < length && isStrWhiteSpace(data[p]))
        ++p;

    double sign = 1;
    if (p < length) {
        if (data[p] == '+')
            ++p;
        else if (data[p] == '-') {
            sign = -1;
            ++p;
        }
    }

    if ((radix == 0 || radix == 16) && length - p >= 2 && data[p] == '0' && (data[p + 1] == 'x' || data[p + 1] == 'X')) {
        radix = 16;
        p += 2;
    } else if (radix == 0) {
        if (p < length && data[p] == '0')
            radix = 8;
        else
            radix = 10;
    }

    if (radix < 2 || radix > 36)
        return NaN;

    int firstDigitPosition = p;
    bool sawDigit = false;
    double number = 0;
    while (p < length) {
        int digit = parseDigit(data[p], radix);
        if (digit == -1)
            break;
        sawDigit = true;
        number *= radix;
        number += digit;
        ++p;
    }

    if (number >= mantissaOverflowLowerBound) {
        if (radix == 10)
            number = WTF::strtod(s.substr(firstDigitPosition, p - firstDigitPosition).ascii(), 0);
        else if (radix == 2 || radix == 4 || radix == 8 || radix == 16 || radix == 32)
            number = parseIntOverflow(s.substr(firstDigitPosition, p - firstDigitPosition).ascii(), p - firstDigitPosition, radix);
    }

    if (!sawDigit)
        return NaN;

    return sign * number;
}

static double parseFloat(const UString& s)
{
    // Check for 0x prefix here, because toDouble allows it, but we must treat it as 0.
    // Need to skip any whitespace and then one + or - sign.
    int length = s.size();
    const UChar* data = s.data();
    int p = 0;
    while (p < length && isStrWhiteSpace(data[p]))
        ++p;

    if (p < length && (data[p] == '+' || data[p] == '-'))
        ++p;

    if (length - p >= 2 && data[p] == '0' && (data[p + 1] == 'x' || data[p + 1] == 'X'))
        return 0;

    return s.toDouble(true /*tolerant*/, false /* NaN for empty string */);
}

JSValue JSC_HOST_CALL globalFuncEval(ExecState* exec, JSObject* function, JSValue thisValue, const ArgList& args)
{
    JSObject* thisObject = thisValue.toThisObject(exec);
    JSObject* unwrappedObject = thisObject->unwrappedObject();
    if (!unwrappedObject->isGlobalObject() || static_cast<JSGlobalObject*>(unwrappedObject)->evalFunction() != function)
        return throwError(exec, EvalError, "The \"this\" value passed to eval must be the global object from which eval originated");

    JSValue x = args.at(0);
    if (!x.isString())
        return x;

    UString s = x.toString(exec);

    LiteralParser preparser(exec, s, LiteralParser::NonStrictJSON);
    if (JSValue parsedObject = preparser.tryLiteralParse())
        return parsedObject;

    int errLine;
    UString errMsg;

    SourceCode source = makeSource(s);
    RefPtr<EvalNode> evalNode = exec->globalData().parser->parse<EvalNode>(exec, exec->dynamicGlobalObject()->debugger(), source, &errLine, &errMsg);

    if (!evalNode)
        return throwError(exec, SyntaxError, errMsg, errLine, source.provider()->asID(), NULL);

    return exec->interpreter()->execute(evalNode.get(), exec, thisObject, static_cast<JSGlobalObject*>(unwrappedObject)->globalScopeChain().node(), exec->exceptionSlot());
}

JSValue JSC_HOST_CALL globalFuncParseInt(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    JSValue value = args.at(0);
    int32_t radix = args.at(1).toInt32(exec);

    if (radix != 0 && radix != 10)
        return jsNumber(exec, parseInt(value.toString(exec), radix));

    if (value.isInt32())
        return value;

    if (value.isDouble()) {
        double d = value.asDouble();
        if (isfinite(d))
            return jsNumber(exec, (d > 0) ? floor(d) : ceil(d));
        if (isnan(d) || isinf(d))
            return jsNaN(exec);
        return jsNumber(exec, 0);
    }

    return jsNumber(exec, parseInt(value.toString(exec), radix));
}

JSValue JSC_HOST_CALL globalFuncParseFloat(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return jsNumber(exec, parseFloat(args.at(0).toString(exec)));
}

JSValue JSC_HOST_CALL globalFuncIsNaN(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return jsBoolean(isnan(args.at(0).toNumber(exec)));
}

JSValue JSC_HOST_CALL globalFuncIsFinite(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    double n = args.at(0).toNumber(exec);
    return jsBoolean(!isnan(n) && !isinf(n));
}

JSValue JSC_HOST_CALL globalFuncDecodeURI(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    static const char do_not_unescape_when_decoding_URI[] =
        "#$&+,/:;=?@";

    return decode(exec, args, do_not_unescape_when_decoding_URI, true);
}

JSValue JSC_HOST_CALL globalFuncDecodeURIComponent(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return decode(exec, args, "", true);
}

JSValue JSC_HOST_CALL globalFuncEncodeURI(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    static const char do_not_escape_when_encoding_URI[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!#$&'()*+,-./:;=?@_~";

    return encode(exec, args, do_not_escape_when_encoding_URI);
}

JSValue JSC_HOST_CALL globalFuncEncodeURIComponent(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    static const char do_not_escape_when_encoding_URI_component[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!'()*-._~";

    return encode(exec, args, do_not_escape_when_encoding_URI_component);
}

JSValue JSC_HOST_CALL globalFuncEscape(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    static const char do_not_escape[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "*+-./@_";

    UString result = "";
    UString s;
    UString str = args.at(0).toString(exec);
    const UChar* c = str.data();
    for (int k = 0; k < str.size(); k++, c++) {
        int u = c[0];
        if (u > 255) {
            char tmp[7];
            sprintf(tmp, "%%u%04X", u);
            s = UString(tmp);
        } else if (u != 0 && strchr(do_not_escape, static_cast<char>(u)))
            s = UString(c, 1);
        else {
            char tmp[4];
            sprintf(tmp, "%%%02X", u);
            s = UString(tmp);
        }
        result += s;
    }

    return jsString(exec, result);
}

JSValue JSC_HOST_CALL globalFuncUnescape(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    UString result = "";
    UString str = args.at(0).toString(exec);
    int k = 0;
    int len = str.size();
    while (k < len) {
        const UChar* c = str.data() + k;
        UChar u;
        if (c[0] == '%' && k <= len - 6 && c[1] == 'u') {
            if (isASCIIHexDigit(c[2]) && isASCIIHexDigit(c[3]) && isASCIIHexDigit(c[4]) && isASCIIHexDigit(c[5])) {
                u = Lexer::convertUnicode(c[2], c[3], c[4], c[5]);
                c = &u;
                k += 5;
            }
        } else if (c[0] == '%' && k <= len - 3 && isASCIIHexDigit(c[1]) && isASCIIHexDigit(c[2])) {
            u = UChar(Lexer::convertHex(c[1], c[2]));
            c = &u;
            k += 2;
        }
        k++;
        result.append(*c);
    }

    return jsString(exec, result);
}

#ifndef NDEBUG
JSValue JSC_HOST_CALL globalFuncJSCPrint(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    CStringBuffer string;
    args.at(0).toString(exec).getCString(string);
    puts(string.data());
    return jsUndefined();
}
#endif

} // namespace JSC
