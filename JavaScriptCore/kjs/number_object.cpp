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
#include "number_object.h"
#include "number_object.lut.h"

#include "dtoa.h"
#include "error_object.h"
#include "operations.h"
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace KJS {

// ------------------------------ NumberInstance ----------------------------

const ClassInfo NumberInstance::info = { "Number", 0, 0 };

NumberInstance::NumberInstance(JSObject* proto)
    : JSWrapperObject(proto)
{
}

// ------------------------------ NumberPrototype ---------------------------

static JSValue* numberProtoFuncToString(ExecState*, JSObject*, const List&);
static JSValue* numberProtoFuncToLocaleString(ExecState*, JSObject*, const List&);
static JSValue* numberProtoFuncValueOf(ExecState*, JSObject*, const List&);
static JSValue* numberProtoFuncToFixed(ExecState*, JSObject*, const List&);
static JSValue* numberProtoFuncToExponential(ExecState*, JSObject*, const List&);
static JSValue* numberProtoFuncToPrecision(ExecState*, JSObject*, const List&);

// ECMA 15.7.4

NumberPrototype::NumberPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : NumberInstance(objectPrototype)
{
    setInternalValue(jsNumber(0));

    // The constructor will be added later, after NumberObjectImp has been constructed

    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().toString, numberProtoFuncToString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toLocaleString, numberProtoFuncToLocaleString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().valueOf, numberProtoFuncValueOf), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().toFixed, numberProtoFuncToFixed), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().toExponential, numberProtoFuncToExponential), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().toPrecision, numberProtoFuncToPrecision), DontEnum);
}

// ------------------------------ Functions ---------------------------

// ECMA 15.7.4.2 - 15.7.4.7

static UString integer_part_noexp(double d)
{
    int decimalPoint;
    int sign;
    char* result = kjs_dtoa(d, 0, 0, &decimalPoint, &sign, NULL);
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
        str += UString(buf.data());
    }

    kjs_freedtoa(result);

    return str;
}

static UString char_sequence(char c, int count)
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


JSValue* numberProtoFuncToString(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&NumberInstance::info))
        return throwError(exec, TypeError);

    JSValue* v = static_cast<NumberInstance*>(thisObj)->internalValue();

    double radixAsDouble = args[0]->toInteger(exec); // nan -> 0
    if (radixAsDouble == 10 || args[0]->isUndefined())
        return jsString(v->toString(exec));

    if (radixAsDouble < 2 || radixAsDouble > 36)
        return throwError(exec, RangeError, "toString() radix argument must be between 2 and 36");

    int radix = static_cast<int>(radixAsDouble);
    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    // INT_MAX results in 1024 characters left of the dot with radix 2
    // give the same space on the right side. safety checks are in place
    // unless someone finds a precise rule.
    char s[2048 + 3];
    const char* lastCharInString = s + sizeof(s) - 1;
    double x = v->toNumber(exec);
    if (isnan(x) || isinf(x))
        return jsString(UString::from(x));

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

    return jsString(startOfResultString);
}

JSValue* numberProtoFuncToLocaleString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&NumberInstance::info))
        return throwError(exec, TypeError);

    // TODO
    return jsString(static_cast<NumberInstance*>(thisObj)->internalValue()->toString(exec));
}

JSValue* numberProtoFuncValueOf(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&NumberInstance::info))
        return throwError(exec, TypeError);

    return static_cast<NumberInstance*>(thisObj)->internalValue()->toJSNumber(exec);
}

JSValue* numberProtoFuncToFixed(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&NumberInstance::info))
        return throwError(exec, TypeError);

    JSValue* v = static_cast<NumberInstance*>(thisObj)->internalValue();

    JSValue* fractionDigits = args[0];
    double df = fractionDigits->toInteger(exec);
    if (!(df >= 0 && df <= 20))
        return throwError(exec, RangeError, "toFixed() digits argument must be between 0 and 20");
    int f = (int)df;

    double x = v->toNumber(exec);
    if (isnan(x))
        return jsString("NaN");

    UString s;
    if (x < 0) {
        s.append('-');
        x = -x;
    } else if (x == -0.0)
        x = 0;

    if (x >= pow(10.0, 21.0))
        return jsString(s + UString::from(x));

    const double tenToTheF = pow(10.0, f);
    double n = floor(x * tenToTheF);
    if (fabs(n / tenToTheF - x) >= fabs((n + 1) / tenToTheF - x))
        n++;

    UString m = integer_part_noexp(n);

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
        return jsString(s + m.substr(0, kMinusf) + "." + m.substr(kMinusf));
    return jsString(s + m.substr(0, kMinusf));
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
    buf[i++] = (decimalPoint >= 0) ? '+' : '-';
    // decimalPoint can't be more than 3 digits decimal given the
    // nature of float representation
    int exponential = decimalPoint - 1;
    if (exponential < 0)
        exponential *= -1;
    if (exponential >= 100)
        buf[i++] = static_cast<char>('0' + exponential / 100);
    if (exponential >= 10)
        buf[i++] = static_cast<char>('0' + (exponential % 100) / 10);
    buf[i++] = static_cast<char>('0' + exponential % 10);
}

JSValue* numberProtoFuncToExponential(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&NumberInstance::info))
        return throwError(exec, TypeError);

    JSValue* v = static_cast<NumberInstance*>(thisObj)->internalValue();

    double x = v->toNumber(exec);

    if (isnan(x) || isinf(x))
        return jsString(UString::from(x));

    JSValue* fractionalDigitsValue = args[0];
    double df = fractionalDigitsValue->toInteger(exec);
    if (!(df >= 0 && df <= 20))
        return throwError(exec, RangeError, "toExponential() argument must between 0 and 20");
    int fractionalDigits = (int)df;
    bool includeAllDigits = fractionalDigitsValue->isUndefined();

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
        return jsString("NaN");

    if (x == -0.0) // (-0.0).toExponential() should print as 0 instead of -0
        x = 0;

    int decimalPoint;
    int sign;
    char* result = kjs_dtoa(x, 0, 0, &decimalPoint, &sign, NULL);
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

    kjs_freedtoa(result);

    return jsString(buf);
}

JSValue* numberProtoFuncToPrecision(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&NumberInstance::info))
        return throwError(exec, TypeError);

    JSValue* v = static_cast<NumberInstance*>(thisObj)->internalValue();

    double doublePrecision = args[0]->toIntegerPreserveNaN(exec);
    double x = v->toNumber(exec);
    if (args[0]->isUndefined() || isnan(x) || isinf(x))
        return jsString(v->toString(exec));

    UString s;
    if (x < 0) {
        s = "-";
        x = -x;
    }

    if (!(doublePrecision >= 1 && doublePrecision <= 21)) // true for NaN
        return throwError(exec, RangeError, "toPrecision() argument must be between 1 and 21");
    int precision = (int)doublePrecision;

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

        m = integer_part_noexp(n);
        if (e < -6 || e >= precision) {
            if (m.size() > 1)
                m = m.substr(0, 1) + "." + m.substr(1);
            if (e >= 0)
                return jsString(s + m + "e+" + UString::from(e));
            return jsString(s + m + "e-" + UString::from(-e));
        }
    } else {
        m = char_sequence('0', precision);
        e = 0;
    }

    if (e == precision - 1)
        return jsString(s + m);
    if (e >= 0) {
        if (e + 1 < m.size())
            return jsString(s + m.substr(0, e + 1) + "." + m.substr(e + 1));
        return jsString(s + m);
    }
    return jsString(s + "0." + char_sequence('0', -(e + 1)) + m);
}

// ------------------------------ NumberObjectImp ------------------------------

const ClassInfo NumberObjectImp::info = { "Function", &InternalFunctionImp::info, &numberTable };

/* Source for number_object.lut.h
@begin numberTable 5
  NaN                   NumberObjectImp::NaNValue       DontEnum|DontDelete|ReadOnly
  NEGATIVE_INFINITY     NumberObjectImp::NegInfinity    DontEnum|DontDelete|ReadOnly
  POSITIVE_INFINITY     NumberObjectImp::PosInfinity    DontEnum|DontDelete|ReadOnly
  MAX_VALUE             NumberObjectImp::MaxValue       DontEnum|DontDelete|ReadOnly
  MIN_VALUE             NumberObjectImp::MinValue       DontEnum|DontDelete|ReadOnly
@end
*/
NumberObjectImp::NumberObjectImp(ExecState* exec, FunctionPrototype* funcProto, NumberPrototype* numberProto)
    : InternalFunctionImp(funcProto, numberProto->classInfo()->className)
{
    // Number.Prototype
    putDirect(exec->propertyNames().prototype, numberProto, DontEnum|DontDelete|ReadOnly);

    // no. of arguments for constructor
    putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}

bool NumberObjectImp::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<NumberObjectImp, InternalFunctionImp>(exec, &numberTable, this, propertyName, slot);
}

JSValue* NumberObjectImp::getValueProperty(ExecState*, int token) const
{
    // ECMA 15.7.3
    switch (token) {
        case NaNValue:
            return jsNaN();
        case NegInfinity:
            return jsNumberCell(-Inf);
        case PosInfinity:
            return jsNumberCell(Inf);
        case MaxValue:
            return jsNumberCell(1.7976931348623157E+308);
        case MinValue:
            return jsNumberCell(5E-324);
    }
    ASSERT_NOT_REACHED();
    return jsNull();
}

bool NumberObjectImp::implementsConstruct() const
{
    return true;
}

// ECMA 15.7.1
JSObject* NumberObjectImp::construct(ExecState* exec, const List& args)
{
    JSObject* proto = exec->lexicalGlobalObject()->numberPrototype();
    NumberInstance* obj = new NumberInstance(proto);

    // FIXME: Check args[0]->isUndefined() instead of args.isEmpty()?
    double n = args.isEmpty() ? 0 : args[0]->toNumber(exec);
    obj->setInternalValue(jsNumber(n));
    return obj;
}

// ECMA 15.7.2
JSValue* NumberObjectImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    // FIXME: Check args[0]->isUndefined() instead of args.isEmpty()?
    return jsNumber(args.isEmpty() ? 0 : args[0]->toNumber(exec));
}

} // namespace KJS
