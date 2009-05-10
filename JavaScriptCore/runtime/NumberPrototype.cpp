/*
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "NumberPrototype.h"

#include "Error.h"
#include "JSFunction.h"
#include "JSString.h"
#include "PrototypeFunction.h"
#include "dtoa.h"
#include "Operations.h"
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(NumberPrototype);

static JSValue JSC_HOST_CALL numberProtoFuncToString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL numberProtoFuncToLocaleString(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL numberProtoFuncValueOf(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL numberProtoFuncToFixed(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL numberProtoFuncToExponential(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL numberProtoFuncToPrecision(ExecState*, JSObject*, JSValue, const ArgList&);

// ECMA 15.7.4

NumberPrototype::NumberPrototype(ExecState* exec, PassRefPtr<Structure> structure, Structure* prototypeFunctionStructure)
    : NumberObject(structure)
{
    setInternalValue(jsNumber(exec, 0));

    // The constructor will be added later, after NumberConstructor has been constructed

    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().toString, numberProtoFuncToString), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 0, exec->propertyNames().toLocaleString, numberProtoFuncToLocaleString), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 0, exec->propertyNames().valueOf, numberProtoFuncValueOf), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().toFixed, numberProtoFuncToFixed), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().toExponential, numberProtoFuncToExponential), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, prototypeFunctionStructure, 1, exec->propertyNames().toPrecision, numberProtoFuncToPrecision), DontEnum);
}

// ------------------------------ Functions ---------------------------

// ECMA 15.7.4.2 - 15.7.4.7

static UString integerPartNoExp(double d)
{
    int decimalPoint;
    int sign;
    char result[80];
    WTF::dtoa(result, d, 0, &decimalPoint, &sign, NULL);
    bool resultIsInfOrNan = (decimalPoint == 9999);
    size_t length = strlen(result);

    UString str = sign ? "-" : "";
    if (resultIsInfOrNan)
        str += result;
    else if (decimalPoint <= 0)
        str += "0";
    else {
        Vector<char, 1024> buf(decimalPoint + 1);

        if (static_cast<int>(length) <= decimalPoint) {
            strcpy(buf.data(), result);
            memset(buf.data() + length, '0', decimalPoint - length);
        } else
            strncpy(buf.data(), result, decimalPoint);

        buf[decimalPoint] = '\0';
        str.append(buf.data());
    }

    return str;
}

static UString charSequence(char c, int count)
{
    Vector<char, 2048> buf(count + 1, c);
    buf[count] = '\0';

    return UString(buf.data());
}

static double intPow10(int e)
{
    // This function uses the "exponentiation by squaring" algorithm and
    // long double to quickly and precisely calculate integer powers of 10.0.

    // This is a handy workaround for <rdar://problem/4494756>

    if (e == 0)
        return 1.0;

    bool negative = e < 0;
    unsigned exp = negative ? -e : e;

    long double result = 10.0;
    bool foundOne = false;
    for (int bit = 31; bit >= 0; bit--) {
        if (!foundOne) {
            if ((exp >> bit) & 1)
                foundOne = true;
        } else {
            result = result * result;
            if ((exp >> bit) & 1)
                result = result * 10.0;
        }
    }

    if (negative)
        return static_cast<double>(1.0 / result);
    return static_cast<double>(result);
}

JSValue JSC_HOST_CALL numberProtoFuncToString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSValue v = thisValue.getJSNumber();
    if (!v)
        return throwError(exec, TypeError);

    double radixAsDouble = args.at(0).toInteger(exec); // nan -> 0
    if (radixAsDouble == 10 || args.at(0).isUndefined())
        return jsString(exec, v.toString(exec));

    if (radixAsDouble < 2 || radixAsDouble > 36)
        return throwError(exec, RangeError, "toString() radix argument must be between 2 and 36");

    int radix = static_cast<int>(radixAsDouble);
    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    // INT_MAX results in 1024 characters left of the dot with radix 2
    // give the same space on the right side. safety checks are in place
    // unless someone finds a precise rule.
    char s[2048 + 3];
    const char* lastCharInString = s + sizeof(s) - 1;
    double x = v.uncheckedGetNumber();
    if (isnan(x) || isinf(x))
        return jsString(exec, UString::from(x));

    bool isNegative = x < 0.0;
    if (isNegative)
        x = -x;

    double integerPart = floor(x);
    char* decimalPoint = s + sizeof(s) / 2;

    // convert integer portion
    char* p = decimalPoint;
    double d = integerPart;
    do {
        int remainderDigit = static_cast<int>(fmod(d, radix));
        *--p = digits[remainderDigit];
        d /= radix;
    } while ((d <= -1.0 || d >= 1.0) && s < p);

    if (isNegative)
        *--p = '-';
    char* startOfResultString = p;
    ASSERT(s <= startOfResultString);

    d = x - integerPart;
    p = decimalPoint;
    const double epsilon = 0.001; // TODO: guessed. base on radix ?
    bool hasFractionalPart = (d < -epsilon || d > epsilon);
    if (hasFractionalPart) {
        *p++ = '.';
        do {
            d *= radix;
            const int digit = static_cast<int>(d);
            *p++ = digits[digit];
            d -= digit;
        } while ((d < -epsilon || d > epsilon) && p < lastCharInString);
    }
    *p = '\0';
    ASSERT(p < s + sizeof(s));

    return jsString(exec, startOfResultString);
}

JSValue JSC_HOST_CALL numberProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    // FIXME: Not implemented yet.

    JSValue v = thisValue.getJSNumber();
    if (!v)
        return throwError(exec, TypeError);

    return jsString(exec, v.toString(exec));
}

JSValue JSC_HOST_CALL numberProtoFuncValueOf(ExecState* exec, JSObject*, JSValue thisValue, const ArgList&)
{
    JSValue v = thisValue.getJSNumber();
    if (!v)
        return throwError(exec, TypeError);

    return v;
}

JSValue JSC_HOST_CALL numberProtoFuncToFixed(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSValue v = thisValue.getJSNumber();
    if (!v)
        return throwError(exec, TypeError);

    JSValue fractionDigits = args.at(0);
    double df = fractionDigits.toInteger(exec);
    if (!(df >= 0 && df <= 20))
        return throwError(exec, RangeError, "toFixed() digits argument must be between 0 and 20");
    int f = static_cast<int>(df);

    double x = v.uncheckedGetNumber();
    if (isnan(x))
        return jsNontrivialString(exec, "NaN");

    UString s;
    if (x < 0) {
        s.append('-');
        x = -x;
    } else if (x == -0.0)
        x = 0;

    if (x >= pow(10.0, 21.0))
        return jsString(exec, s + UString::from(x));

    const double tenToTheF = pow(10.0, f);
    double n = floor(x * tenToTheF);
    if (fabs(n / tenToTheF - x) >= fabs((n + 1) / tenToTheF - x))
        n++;

    UString m = integerPartNoExp(n);

    int k = m.size();
    if (k <= f) {
        UString z;
        for (int i = 0; i < f + 1 - k; i++)
            z.append('0');
        m = z + m;
        k = f + 1;
        ASSERT(k == m.size());
    }
    int kMinusf = k - f;
    if (kMinusf < m.size())
        return jsString(exec, s + m.substr(0, kMinusf) + "." + m.substr(kMinusf));
    return jsString(exec, s + m.substr(0, kMinusf));
}

static void fractionalPartToString(char* buf, int& i, const char* result, int resultLength, int fractionalDigits)
{
    if (fractionalDigits <= 0)
        return;

    int fDigitsInResult = static_cast<int>(resultLength) - 1;
    buf[i++] = '.';
    if (fDigitsInResult > 0) {
        if (fractionalDigits < fDigitsInResult) {
            strncpy(buf + i, result + 1, fractionalDigits);
            i += fractionalDigits;
        } else {
            strcpy(buf + i, result + 1);
            i += static_cast<int>(resultLength) - 1;
        }
    }

    for (int j = 0; j < fractionalDigits - fDigitsInResult; j++)
        buf[i++] = '0';
}

static void exponentialPartToString(char* buf, int& i, int decimalPoint)
{
    buf[i++] = 'e';
    // decimalPoint can't be more than 3 digits decimal given the
    // nature of float representation
    int exponential = decimalPoint - 1;
    buf[i++] = (exponential >= 0) ? '+' : '-';
    if (exponential < 0)
        exponential *= -1;
    if (exponential >= 100)
        buf[i++] = static_cast<char>('0' + exponential / 100);
    if (exponential >= 10)
        buf[i++] = static_cast<char>('0' + (exponential % 100) / 10);
    buf[i++] = static_cast<char>('0' + exponential % 10);
}

JSValue JSC_HOST_CALL numberProtoFuncToExponential(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSValue v = thisValue.getJSNumber();
    if (!v)
        return throwError(exec, TypeError);

    double x = v.uncheckedGetNumber();

    if (isnan(x) || isinf(x))
        return jsString(exec, UString::from(x));

    JSValue fractionalDigitsValue = args.at(0);
    double df = fractionalDigitsValue.toInteger(exec);
    if (!(df >= 0 && df <= 20))
        return throwError(exec, RangeError, "toExponential() argument must between 0 and 20");
    int fractionalDigits = static_cast<int>(df);
    bool includeAllDigits = fractionalDigitsValue.isUndefined();

    int decimalAdjust = 0;
    if (x && !includeAllDigits) {
        double logx = floor(log10(fabs(x)));
        x /= pow(10.0, logx);
        const double tenToTheF = pow(10.0, fractionalDigits);
        double fx = floor(x * tenToTheF) / tenToTheF;
        double cx = ceil(x * tenToTheF) / tenToTheF;

        if (fabs(fx - x) < fabs(cx - x))
            x = fx;
        else
            x = cx;

        decimalAdjust = static_cast<int>(logx);
    }

    if (isnan(x))
        return jsNontrivialString(exec, "NaN");

    if (x == -0.0) // (-0.0).toExponential() should print as 0 instead of -0
        x = 0;

    int decimalPoint;
    int sign;
    char result[80];
    WTF::dtoa(result, x, 0, &decimalPoint, &sign, NULL);
    size_t resultLength = strlen(result);
    decimalPoint += decimalAdjust;

    int i = 0;
    char buf[80]; // digit + '.' + fractionDigits (max 20) + 'e' + sign + exponent (max?)
    if (sign)
        buf[i++] = '-';

    if (decimalPoint == 999) // ? 9999 is the magical "result is Inf or NaN" value.  what's 999??
        strcpy(buf + i, result);
    else {
        buf[i++] = result[0];

        if (includeAllDigits)
            fractionalDigits = static_cast<int>(resultLength) - 1;

        fractionalPartToString(buf, i, result, resultLength, fractionalDigits);
        exponentialPartToString(buf, i, decimalPoint);
        buf[i++] = '\0';
    }
    ASSERT(i <= 80);

    return jsString(exec, buf);
}

JSValue JSC_HOST_CALL numberProtoFuncToPrecision(ExecState* exec, JSObject*, JSValue thisValue, const ArgList& args)
{
    JSValue v = thisValue.getJSNumber();
    if (!v)
        return throwError(exec, TypeError);

    double doublePrecision = args.at(0).toIntegerPreserveNaN(exec);
    double x = v.uncheckedGetNumber();
    if (args.at(0).isUndefined() || isnan(x) || isinf(x))
        return jsString(exec, v.toString(exec));

    UString s;
    if (x < 0) {
        s = "-";
        x = -x;
    }

    if (!(doublePrecision >= 1 && doublePrecision <= 21)) // true for NaN
        return throwError(exec, RangeError, "toPrecision() argument must be between 1 and 21");
    int precision = static_cast<int>(doublePrecision);

    int e = 0;
    UString m;
    if (x) {
        e = static_cast<int>(log10(x));
        double tens = intPow10(e - precision + 1);
        double n = floor(x / tens);
        if (n < intPow10(precision - 1)) {
            e = e - 1;
            tens = intPow10(e - precision + 1);
            n = floor(x / tens);
        }

        if (fabs((n + 1.0) * tens - x) <= fabs(n * tens - x))
            ++n;
        // maintain n < 10^(precision)
        if (n >= intPow10(precision)) {
            n /= 10.0;
            e += 1;
        }
        ASSERT(intPow10(precision - 1) <= n);
        ASSERT(n < intPow10(precision));

        m = integerPartNoExp(n);
        if (e < -6 || e >= precision) {
            if (m.size() > 1)
                m = m.substr(0, 1) + "." + m.substr(1);
            if (e >= 0)
                return jsNontrivialString(exec, s + m + "e+" + UString::from(e));
            return jsNontrivialString(exec, s + m + "e-" + UString::from(-e));
        }
    } else {
        m = charSequence('0', precision);
        e = 0;
    }

    if (e == precision - 1)
        return jsString(exec, s + m);
    if (e >= 0) {
        if (e + 1 < m.size())
            return jsString(exec, s + m.substr(0, e + 1) + "." + m.substr(e + 1));
        return jsString(exec, s + m);
    }
    return jsNontrivialString(exec, s + "0." + charSequence('0', -(e + 1)) + m);
}

} // namespace JSC
