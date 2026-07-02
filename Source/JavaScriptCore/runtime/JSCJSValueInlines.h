/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueBigInt.h>
#include <JavaScriptCore/JSCJSValuePropertyInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSStringInlines.h>
#include <JavaScriptCore/ThrowScope.h>
#include <wtf/MediaTime.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

namespace JSC {

inline JSValue jsNumber(double d)
{
    ASSERT(JSValue(d).isNumber());
    ASSERT(!isImpureNaN(d));
    return JSValue(d);
}

inline JSValue jsNumber(const MediaTime& t)
{
    return jsNumber(t.toDouble());
}

inline JSValue::JSValue(double d)
{
    if (auto int32Value = tryConvertToStrictInt32(d)) {
        *this = JSValue(int32Value.value());
        return;
    }
    *this = JSValue(EncodeAsDouble, d);
}

ALWAYS_INLINE int32_t JSValue::toInt32(JSGlobalObject* globalObject) const
{
    if (isInt32())
        return asInt32();

    double d = toNumber(globalObject);
    return JSC::toInt32(d);
}

inline uint32_t JSValue::toUInt32(JSGlobalObject* globalObject) const
{
    // The only difference between toInt32 and toUint32 is that toUint32 reinterprets resulted int32_t value as uint32_t.
    // https://tc39.es/ecma262/#sec-touint32
    return toInt32(globalObject);
}

inline std::optional<uint32_t> JSValue::tryGetAsUint32Index()
{
    if (isUInt32()) {
        ASSERT(isIndex(asUInt32()));
        return asUInt32();
    }
    if (isNumber()) {
        double number = asNumber();
        uint32_t asUint = static_cast<uint32_t>(truncateDoubleToUint64(number));
        if (static_cast<double>(asUint) == number && isIndex(asUint))
            return asUint;
    }
    return std::nullopt;
}

inline std::optional<int32_t> JSValue::tryGetAsInt32()
{
    if (isInt32())
        return asInt32();
    if (isNumber()) {
        double number = asNumber();
        int32_t asInt = truncateDoubleToInt32(number);
        if (static_cast<double>(asInt) == number)
            return asInt;
    }
    return std::nullopt;
}

template <typename Base> String HandleConverter<Base, Unknown>::getString(JSGlobalObject* globalObject) const
{
    return jsValue().getString(globalObject);
}

ALWAYS_INLINE Identifier JSValue::toPropertyKey(JSGlobalObject* globalObject) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isString())
        RELEASE_AND_RETURN(scope, asString(*this)->toIdentifier(globalObject));

    JSValue primitive = toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, vm.propertyNames->emptyIdentifier);
    if (primitive.isSymbol())
        RELEASE_AND_RETURN(scope, Identifier::fromUid(asSymbol(primitive)->privateName()));

    auto string = primitive.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, string->toIdentifier(globalObject));
}

ALWAYS_INLINE JSValue JSValue::toPropertyKeyValue(JSGlobalObject* globalObject) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isString() || isSymbol())
        return *this;

    JSValue primitive = toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (primitive.isSymbol())
        return primitive;

    RELEASE_AND_RETURN(scope, primitive.toString(globalObject));
}

inline PreferredPrimitiveType toPreferredPrimitiveType(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isString()) {
        throwTypeError(globalObject, scope, "Primitive hint is not a string."_s);
        return NoPreference;
    }

    auto hintString = asString(value)->view(globalObject);
    RETURN_IF_EXCEPTION(scope, NoPreference);

    if (WTF::equal(hintString, "default"_s))
        return NoPreference;
    if (WTF::equal(hintString, "number"_s))
        return PreferNumber;
    if (WTF::equal(hintString, "string"_s))
        return PreferString;

    throwTypeError(globalObject, scope, "Expected primitive hint to match one of 'default', 'number', 'string'."_s);
    return NoPreference;
}

ALWAYS_INLINE JSValue JSValue::toNumeric(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isInt32() || isDouble() || isBigInt())
        return *this;

    if (isString())
        RELEASE_AND_RETURN(scope, jsNumber(asString(*this)->toNumber(globalObject)));

    JSValue primValue = this->toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, { });

    if (primValue.isDouble() || primValue.isBigInt())
        return primValue;

    double value = primValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    return jsNumber(value);
}

ALWAYS_INLINE std::optional<uint32_t> JSValue::toUInt32AfterToNumeric(JSGlobalObject* globalObject) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = toBigIntOrInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (result.isInt32()) [[likely]]
        return static_cast<uint32_t>(result.asInt32());
    return std::nullopt;
}

ALWAYS_INLINE JSValue JSValue::toBigIntOrInt32(JSGlobalObject* globalObject) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isInt32() || isBigInt())
        return *this;

    if (isDouble()) {
        if (auto int32Value = tryConvertToStrictInt32(asDouble()))
            return jsNumber(int32Value.value());
    }

    JSValue primValue = this->toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, { });

    if (primValue.isInt32() || primValue.isBigInt())
        return primValue;

    int32_t value = primValue.toInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return jsNumber(value);
}

inline JSValue JSValue::toThis(JSGlobalObject* globalObject, ECMAMode ecmaMode) const
{
    if (isObject()) {
        if (asObject(*this)->inherits<JSScope>())
            return ecmaMode.isStrict() ? jsUndefined() : globalObject->globalThis();
        return *this;
    }

    if (ecmaMode.isStrict())
        return *this;

    ASSERT(!ecmaMode.isStrict());
    if (isUndefinedOrNull())
        return globalObject->globalThis();

    return toThisSloppySlowCase(globalObject);
}

ALWAYS_INLINE bool JSValue::equalSlowCaseInline(JSGlobalObject* globalObject, JSValue v1, JSValue v2)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    do {
        if (v1.isNumber()) {
            if (v2.isNumber())
                return v1.asNumber() == v2.asNumber();
            // Guaranteeing that if we have a number it is v2 makes some of the cases below simpler.
            std::swap(v1, v2);
        }

        // This deals with Booleans, BigInt32, Objects, and is a shortcut for a few more types.
        // It has to come here and not before, because it is NOT true that NaN == NaN
        if (v1 == v2)
            return true;

        if (v1.isUndefinedOrNull()) {
            if (v2.isUndefinedOrNull())
                return true;
            if (!v2.isCell())
                return false;
            return v2.asCell()->structure()->masqueradesAsUndefined(globalObject);
        }

        if (v2.isUndefinedOrNull()) {
            if (!v1.isCell())
                return false;
            return v1.asCell()->structure()->masqueradesAsUndefined(globalObject);
        }

        if (v1.isObject()) {
            if (v2.isObject())
                return false; // v1 == v2 is already dealt with previously
            JSValue p1 = v1.toPrimitive(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            v1 = p1;
            if (v1.isInt32() && v2.isInt32())
                return v1 == v2;
            continue;
        }

        if (v2.isObject()) {
            JSValue p2 = v2.toPrimitive(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            v2 = p2;
            if (v1.isInt32() && v2.isInt32())
                return v1 == v2;
            continue;
        }

        bool sym1 = v1.isSymbol();
        bool sym2 = v2.isSymbol();
        if (sym1 || sym2) {
            if (sym1 && sym2)
                return false; // v1 == v2 is already dealt with previously
            return false;
        }

        bool s1 = v1.isString();
        bool s2 = v2.isString();
        if (s1) {
            if (s2)
                RELEASE_AND_RETURN(scope, asString(v1)->equal(globalObject, asString(v2)));
            std::swap(v1, v2);
            // We are guaranteed to enter the next case, so losing the invariant of only v2 being a number is fine
        }
        if (s1 || s2) {
            // We are guaranteed that the string is v2 (thanks to the swap above)
            if (v1.isBigInt()) {
                auto v2String = asString(v2)->value(globalObject);
                RETURN_IF_EXCEPTION(scope, false);
                v2 = JSBigInt::stringToBigInt(globalObject, v2String);
                RETURN_IF_EXCEPTION(scope, false);
                if (!v2)
                    return false;
                if (v1 == v2)
                    return true; // For BigInt32
                // We fallthrough to the generic code for comparing BigInts (which is only missing the BigInt32/BigInt32 case, hence the check above)
            } else {
                ASSERT(v1.isNumber() || v1.isBoolean());
                double d1 = v1.toNumber(globalObject);
                RETURN_IF_EXCEPTION(scope, false);
                double d2 = v2.toNumber(globalObject);
                RETURN_IF_EXCEPTION(scope, false);
                return d1 == d2;
            }
        }

        if (v1.isBoolean()) {
            if (v2.isNumber())
                return static_cast<double>(v1.asBoolean()) == v2.asNumber();
            v1 = JSValue(v1.toNumber(globalObject));
            RETURN_IF_EXCEPTION(scope, false);
            // We fallthrough to the BigInt/Number comparison below
            // We just need one more swap to repair the rule that only v2 is allowed to be a number in these comparisons
            std::swap(v1, v2);
        } else if (v2.isBoolean()) {
            v2 = JSValue(v2.toNumber(globalObject));
            RETURN_IF_EXCEPTION(scope, false);
            // We fallthrough to the BigInt/Number comparison below
        }

#if USE(BIGINT32)
        if (v1.isBigInt32()) {
            if (v2.isInt32())
                return v1.bigInt32AsInt32() == v2.asInt32();
            if (v2.isDouble())
                return static_cast<double>(v1.bigInt32AsInt32()) == v2.asDouble();
            std::swap(v1, v2);
        }
#endif // USE(BIGINT32)

        if (v1.isHeapBigInt()) {
            if (v2.isHeapBigInt())
                return JSBigInt::equals(v1.asHeapBigInt(), v2.asHeapBigInt());
#if USE(BIGINT32)
            if (v2.isBigInt32())
                return v1.asHeapBigInt()->equalsToInt32(v2.bigInt32AsInt32());
#endif // USE(BIGINT32)
            if (v2.isNumber())
                return v1.asHeapBigInt()->equalsToNumber(v2);
        }

        return false;
    } while (true);
}

// ECMA 11.9.3
ALWAYS_INLINE bool JSValue::strictEqualForCells(JSGlobalObject* globalObject, JSCell* v1, JSCell* v2)
{
    if (v1->isString() && v2->isString())
        return asString(v1)->equal(globalObject, asString(v2));
    if (v1->isHeapBigInt() && v2->isHeapBigInt())
        return JSBigInt::equals(static_cast<JSBigInt*>(v1), static_cast<JSBigInt*>(v2));
    return v1 == v2;
}

inline bool JSValue::strictEqual(JSGlobalObject* globalObject, JSValue v1, JSValue v2)
{
    if (v1.isInt32() && v2.isInt32())
        return v1 == v2;

    if (v1.isNumber() && v2.isNumber())
        return v1.asNumber() == v2.asNumber();

#if USE(BIGINT32)
    if (v1.isHeapBigInt() && v2.isBigInt32())
        return v1.asHeapBigInt()->equalsToInt32(v2.bigInt32AsInt32());
    if (v1.isBigInt32() && v2.isHeapBigInt())
        return v2.asHeapBigInt()->equalsToInt32(v1.bigInt32AsInt32());
#endif

    if (v1.isCell() && v2.isCell())
        return strictEqualForCells(globalObject, v1.asCell(), v2.asCell());

    return v1 == v2;
}

inline TriState JSValue::pureStrictEqual(JSValue v1, JSValue v2)
{
    if (v1.isInt32() && v2.isInt32())
        return triState(v1 == v2);

    if (v1.isNumber() && v2.isNumber())
        return triState(v1.asNumber() == v2.asNumber());

#if USE(BIGINT32)
    if (v1.isHeapBigInt() && v2.isBigInt32())
        return triState(v1.asHeapBigInt()->equalsToInt32(v2.bigInt32AsInt32()));
    if (v1.isBigInt32() && v2.isHeapBigInt())
        return triState(v2.asHeapBigInt()->equalsToInt32(v1.bigInt32AsInt32()));
#endif

    if (v1.isCell() && v2.isCell()) {
        if (v1.asCell()->isString() && v2.asCell()->isString()) {
            const StringImpl* v1String = asString(v1)->tryGetValueImpl();
            const StringImpl* v2String = asString(v2)->tryGetValueImpl();
            if (!v1String || !v2String)
                return TriState::Indeterminate;
            return triState(WTF::equal(*v1String, *v2String));
        }
        if (v1.asCell()->isHeapBigInt() && v2.asCell()->isHeapBigInt())
            return triState(JSBigInt::equals(v1.asHeapBigInt(), v2.asHeapBigInt()));
    }
    
    return triState(v1 == v2);
}

inline TriState JSValue::pureToBoolean() const
{
    if (isInt32())
        return asInt32() ? TriState::True : TriState::False;
    if (isDouble())
        return isNotZeroAndOrdered(asDouble()) ? TriState::True : TriState::False; // false for NaN
    if (isCell())
        return asCell()->pureToBoolean();
#if USE(BIGINT32)
    if (isBigInt32())
        return bigInt32AsInt32() ? TriState::True : TriState::False;
#endif
    return isTrue() ? TriState::True : TriState::False;
}

// See section 7.2.9: https://tc39.github.io/ecma262/#sec-samevalue
ALWAYS_INLINE bool sameValue(JSGlobalObject* globalObject, JSValue a, JSValue b)
{
    if (a == b)
        return true;

    if (!a.isNumber())
        return JSValue::strictEqual(globalObject, a, b);
    if (!b.isNumber())
        return false;
    double x = a.asNumber();
    double y = b.asNumber();
    bool xIsNaN = std::isnan(x);
    bool yIsNaN = std::isnan(y);
    if (xIsNaN || yIsNaN)
        return xIsNaN && yIsNaN;
    return std::bit_cast<uint64_t>(x) == std::bit_cast<uint64_t>(y);
}

ALWAYS_INLINE bool sameValueZero(JSGlobalObject* globalObject, JSValue a, JSValue b)
{
    if (a == b)
        return true;

    if (!a.isNumber())
        return JSValue::strictEqual(globalObject, a, b);
    if (!b.isNumber())
        return false;
    double x = a.asNumber();
    double y = b.asNumber();
    if (std::isnan(x))
        return std::isnan(y);
    if (std::isnan(y))
        return std::isnan(x);
    if (!x && y == -0)
        return true;
    if (x == -0 && !y)
        return true;
    return std::bit_cast<uint64_t>(x) == std::bit_cast<uint64_t>(y);
}

} // namespace JSC
