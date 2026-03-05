/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
 *
 * Parts of the implementation below:
 *
 * Copyright 2017-2021 the V8 project authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 *
 * Copyright (c) 2014 the Dart project authors.  Please see the AUTHORS file [1]
 * for details. All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE file [2].
 *
 * [1] https://github.com/dart-lang/sdk/blob/master/AUTHORS
 * [2] https://github.com/dart-lang/sdk/blob/master/LICENSE
 *
 * Copyright 2009 The Go Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file [3].
 *
 * [3] https://golang.org/LICENSE
 */

#include "config.h"
#include "JSBigInt.h"

#include "BigIntObject.h"
#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "MathCommon.h"
#include "ParseInt.h"
#include "StructureInlines.h"
#include <algorithm>
#include <wtf/Hasher.h>
#include <wtf/Int128.h>
#include <wtf/MathExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ClassInfo JSBigInt::s_info = { "BigInt"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSBigInt) };

JSBigInt::JSBigInt(VM& vm, Structure* structure, unsigned length)
    : Base(vm, structure)
    , m_length(length)
{ }

void JSBigInt::initialize(InitializationType initType)
{
    if (initType == InitializationType::WithZero)
        zeroSpan(digits());
}

Structure* JSBigInt::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(HeapBigIntType, StructureFlags), info());
}

inline JSBigInt* JSBigInt::createZero(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm)
{
    return createWithLength(nullOrGlobalObjectForOOM, vm, 0);
}

JSBigInt* JSBigInt::createZero(JSGlobalObject* globalObject)
{
    return createZero(globalObject, globalObject->vm());
}

JSBigInt* JSBigInt::tryCreateZero(VM& vm)
{
    return createZero(nullptr, vm);
}

inline JSBigInt* JSBigInt::createWithLength(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, unsigned length)
{
    if (length > maxLength) [[unlikely]] {
        if (nullOrGlobalObjectForOOM) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope, "BigInt generated from this operation is too big"_s);
        }
        return nullptr;
    }

    ASSERT(length <= maxLength);
    auto* cell = tryAllocateCell<JSBigInt>(vm, JSBigInt::allocationSize(length));
    if (!cell) [[unlikely]] {
        if (nullOrGlobalObjectForOOM) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
        }
        return nullptr;
    }

    JSBigInt* bigInt = new (NotNull, cell) JSBigInt(vm, vm.bigIntStructure.get(), length);
    bigInt->finishCreation(vm);
    return bigInt;
}

JSBigInt* JSBigInt::tryCreateWithLength(VM& vm, unsigned length)
{
    return createWithLength(nullptr, vm, length);
}

JSBigInt* JSBigInt::createWithLength(JSGlobalObject* globalObject, unsigned length)
{
    return createWithLength(globalObject, globalObject->vm(), length);
}

inline JSBigInt* JSBigInt::createFrom(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, int32_t value)
{
    if (!value)
        return createZero(nullOrGlobalObjectForOOM, vm);

    JSBigInt* bigInt = createWithLength(nullOrGlobalObjectForOOM, vm, 1);
    if (!bigInt) [[unlikely]]
        return nullptr;

    if (value < 0) {
        bigInt->setDigit(0, static_cast<Digit>(-1 * static_cast<int64_t>(value)));
        bigInt->setSign(true);
    } else
        bigInt->setDigit(0, static_cast<Digit>(value));

    return bigInt;
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, int32_t value)
{
    return createFrom(globalObject, globalObject->vm(), value);
}

JSBigInt* JSBigInt::tryCreateFrom(VM& vm, int32_t value)
{
    return createFrom(nullptr, vm, value);
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, uint32_t value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value)
        RELEASE_AND_RETURN(scope, createZero(globalObject));
    
    JSBigInt* bigInt = createWithLength(globalObject, 1);
    RETURN_IF_EXCEPTION(scope, nullptr);
    bigInt->setDigit(0, static_cast<Digit>(value));
    return bigInt;
}

inline JSBigInt* JSBigInt::tryCreateFromImpl(JSGlobalObject* globalObject, uint64_t value, bool sign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value)
        RELEASE_AND_RETURN(scope, createZero(globalObject));

    if (sizeof(Digit) == 8 || value <= UINT32_MAX) {
        JSBigInt* bigInt = createWithLength(globalObject, 1);
        RETURN_IF_EXCEPTION(scope, nullptr);
        bigInt->setDigit(0, static_cast<Digit>(value));
        bigInt->setSign(sign);
        return bigInt;
    }

    ASSERT(sizeof(Digit) == 4);
    JSBigInt* bigInt = createWithLength(globalObject, 2);
    RETURN_IF_EXCEPTION(scope, nullptr);
    Digit lowBits  = static_cast<Digit>(value & 0xffffffff);
    Digit highBits = static_cast<Digit>((value >> 32) & 0xffffffff);

    ASSERT(highBits);

    bigInt->setDigit(0, lowBits);
    bigInt->setDigit(1, highBits);
    bigInt->setSign(sign);

    return bigInt;
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, uint64_t value)
{
    return tryCreateFromImpl(globalObject, value, false);
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, int64_t value)
{
    uint64_t unsignedValue;
    bool sign = false;
    if (value < 0) {
        unsignedValue = static_cast<uint64_t>(-(value + 1)) + 1;
        sign = true;
    } else
        unsignedValue = value;
    return tryCreateFromImpl(globalObject, unsignedValue, sign);
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, Int128 value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value)
        RELEASE_AND_RETURN(scope, createZero(globalObject));

    UInt128 unsignedValue;
    bool sign = false;
    if (value < 0) {
        unsignedValue = static_cast<UInt128>(-(value + 1)) + 1;
        sign = true;
    } else
        unsignedValue = value;

    if (unsignedValue <= UINT64_MAX)
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, static_cast<uint64_t>(unsignedValue), sign));

    if constexpr (sizeof(Digit) == 8) {
        JSBigInt* bigInt = createWithLength(globalObject, 2);
        RETURN_IF_EXCEPTION(scope, nullptr);

        Digit lowBits = static_cast<Digit>(static_cast<uint64_t>(unsignedValue));
        Digit highBits = static_cast<Digit>(static_cast<uint64_t>(unsignedValue >> 64));

        ASSERT(highBits);

        bigInt->setDigit(0, lowBits);
        bigInt->setDigit(1, highBits);
        bigInt->setSign(sign);
        return bigInt;
    }

    ASSERT(sizeof(Digit) == 4);

    Digit digit0 = static_cast<Digit>(static_cast<uint64_t>(unsignedValue));
    Digit digit1 = static_cast<Digit>(static_cast<uint64_t>(unsignedValue >> 32));
    Digit digit2 = static_cast<Digit>(static_cast<uint64_t>(unsignedValue >> 64));
    Digit digit3 = static_cast<Digit>(static_cast<uint64_t>(unsignedValue >> 96));

    ASSERT(digit2 || digit3);

    int length = digit3 ? 4 : 3;
    JSBigInt* bigInt = createWithLength(globalObject, length);
    RETURN_IF_EXCEPTION(scope, nullptr);

    bigInt->setDigit(0, digit0);
    bigInt->setDigit(1, digit1);
    bigInt->setDigit(2, digit2);
    if (digit3)
        bigInt->setDigit(3, digit3);
    bigInt->setSign(sign);
    return bigInt;
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, bool value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value)
        RELEASE_AND_RETURN(scope, createZero(globalObject));

    JSBigInt* bigInt = createWithLength(globalObject, 1);
    RETURN_IF_EXCEPTION(scope, nullptr);
    bigInt->setDigit(0, static_cast<Digit>(value));
    return bigInt;
}

JSBigInt* JSBigInt::createFrom(JSGlobalObject* globalObject, double value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(isInteger(value));
    if (!value)
        RELEASE_AND_RETURN(scope, createZero(globalObject));

    bool sign = value < 0; // -0 was already handled above.
    uint64_t doubleBits = std::bit_cast<uint64_t>(value);
    int32_t rawExponent = static_cast<int32_t>(doubleBits >> doublePhysicalMantissaSize) & 0x7ff;
    ASSERT(rawExponent != 0x7ff); // Since value is integer, exponent should not be 0x7ff (full bits, used for infinity etc.).
    ASSERT(rawExponent >= 0x3ff); // Since value is integer, exponent should be >= 0 + bias (0x3ff).
    int32_t exponent = rawExponent - 0x3ff;
    int32_t digits = exponent / digitBits + 1;
    Vector<Digit, 64> resultVector(digits, 0);
    auto result = resultVector.mutableSpan();

    // We construct a BigInt from the double value by shifting its mantissa
    // according to its exponent and mapping the bit pattern onto digits.
    //
    //               <----------- bitlength = exponent + 1 ----------->
    //                <----- 52 ------> <------ trailing zeroes ------>
    // mantissa:     1yyyyyyyyyyyyyyyyy 0000000000000000000000000000000
    // digits:    0001xxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
    //                <-->          <------>
    //           msdTopBit         digitBits
    //

    uint64_t mantissa = (doubleBits & doublePhysicalMantissaMask) | doubleMantissaHiddenBit;

    int32_t mantissaTopBit = doubleMantissaSize - 1; // 0-indexed.
    // 0-indexed position of most significant bit in the most significant digit.
    int32_t msdTopBit = exponent % digitBits;
    // Number of unused bits in mantissa. We'll keep them shifted to the
    // left (i.e. most significant part) of the underlying uint64_t.
    int32_t remainingMantissaBits = 0;
    // Next digit under construction.
    Digit digit = 0;

    // First, build the MSD by shifting the mantissa appropriately.
    if (msdTopBit < mantissaTopBit) {
        remainingMantissaBits = mantissaTopBit - msdTopBit;
        digit = static_cast<Digit>(mantissa >> remainingMantissaBits);
        mantissa = mantissa << (64 - remainingMantissaBits);
    } else {
        ASSERT(msdTopBit >= mantissaTopBit);
        digit = static_cast<Digit>(mantissa << (msdTopBit - mantissaTopBit));
        mantissa = 0;
    }
    result[digits - 1] = digit;
    // Then fill in the rest of the digits.
    for (int32_t digitIndex = digits - 2; digitIndex >= 0; digitIndex--) {
        if (remainingMantissaBits > 0) {
            remainingMantissaBits -= digitBits;
            if constexpr (sizeof(Digit) == 4) {
                digit = mantissa >> 32;
                mantissa = mantissa << 32;
            } else {
                ASSERT(sizeof(Digit) == 8);
                digit = mantissa;
                mantissa = 0;
            }
        } else
            digit = 0;
        result[digitIndex] = digit;
    }
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, sign, result));
}

JSValue JSBigInt::toPrimitive(JSGlobalObject*, PreferredPrimitiveType) const
{
    return const_cast<JSBigInt*>(this);
}

JSValue JSBigInt::parseInt(JSGlobalObject* globalObject, StringView s, ErrorParseMode parserMode)
{
    if (s.is8Bit())
        return parseInt(globalObject, s.span8(), parserMode);
    return parseInt(globalObject, s.span16(), parserMode);
}

JSValue JSBigInt::parseInt(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, StringView s, uint8_t radix, ErrorParseMode parserMode, ParseIntSign sign)
{
    if (s.is8Bit())
        return parseInt(nullOrGlobalObjectForOOM, vm, s.span8(), 0, radix, parserMode, sign, ParseIntMode::DisallowEmptyString);
    return parseInt(nullOrGlobalObjectForOOM, vm, s.span16(), 0, radix, parserMode, sign, ParseIntMode::DisallowEmptyString);
}

JSValue JSBigInt::stringToBigInt(JSGlobalObject* globalObject, StringView s)
{
    return parseInt(globalObject, s, ErrorParseMode::IgnoreExceptions);
}

String JSBigInt::toString(JSGlobalObject* globalObject, unsigned radix)
{
    if (this->isZero())
        return globalObject->vm().smallStrings.singleCharacterStringRep('0');

    if (hasOneBitSet(radix))
        return toStringBasePowerOfTwo(globalObject->vm(), globalObject, this, radix);

    return toStringGeneric(globalObject->vm(), globalObject, this, radix);
}

String JSBigInt::tryGetString(VM& vm, JSBigInt* bigInt, unsigned radix)
{
    if (bigInt->isZero())
        return vm.smallStrings.singleCharacterStringRep('0');

    if (hasOneBitSet(radix))
        return toStringBasePowerOfTwo(vm, nullptr, bigInt, radix);

    return toStringGeneric(vm, nullptr, bigInt, radix);
}

class HeapBigIntImpl {
public:
    explicit HeapBigIntImpl(JSBigInt* bigInt)
        : m_bigInt(bigInt)
    { }

    ALWAYS_INLINE bool isZero() { return m_bigInt->isZero(); }
    ALWAYS_INLINE bool sign() { return m_bigInt->sign(); }
    ALWAYS_INLINE unsigned length() { return m_bigInt->length(); }
    ALWAYS_INLINE JSBigInt::Digit digit(unsigned i) { return m_bigInt->digit(i); }
    ALWAYS_INLINE std::span<const JSBigInt::Digit> digits() { return { m_bigInt->dataStorage(), m_bigInt->length() }; }
    ALWAYS_INLINE JSBigInt* toHeapBigInt(JSGlobalObject*, VM&) { return m_bigInt; }
    ALWAYS_INLINE JSBigInt* toHeapBigInt(JSGlobalObject*) { return m_bigInt; }

private:
    friend struct JSBigInt::ImplResult;
    JSBigInt* m_bigInt;
};

template<typename D>
static std::span<D> normalize(std::span<D> x)
{
    while (!x.empty() && !x.back())
        x = x.first(x.size() - 1);
    return x;
}

class Int32BigIntImpl {
public:
    explicit Int32BigIntImpl(int32_t value)
        : m_value(value)
    {
        if (!isZero())
            m_digit = digit(0);
    }

    ALWAYS_INLINE bool isZero() { return !m_value; }
    ALWAYS_INLINE bool sign() { return m_value < 0; }
    ALWAYS_INLINE unsigned length() { return isZero() ? 0 : 1; }
    ALWAYS_INLINE JSBigInt::Digit digit(unsigned i)
    {
        ASSERT(length());
        ASSERT_UNUSED(i, i == 0);
        if (sign())
            return static_cast<JSBigInt::Digit>(WTF::negate(static_cast<int64_t>(m_value)));
        return m_value;
    }

    ALWAYS_INLINE std::span<const JSBigInt::Digit> digits() { return { &m_digit, length() }; }

    ALWAYS_INLINE JSBigInt* toHeapBigInt(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm)
    {
        return JSBigInt::createFrom(nullOrGlobalObjectForOOM, vm, m_value);
    }

    ALWAYS_INLINE JSBigInt* toHeapBigInt(JSGlobalObject* globalObject)
    {
        return JSBigInt::createFrom(globalObject, m_value);
    }

private:
    friend struct JSBigInt::ImplResult;
    int32_t m_value;
    JSBigInt::Digit m_digit { };
};

class Int64BigIntImpl {
public:
    static constexpr unsigned numDigits = isRegister64Bit() ? 1 : 2;

    explicit Int64BigIntImpl(int64_t value)
        : m_value(value)
        , m_sign(value < 0)
    {
#if CPU(REGISTER64)
        if (!isZero())
            m_digits[0] = digit(0);
#else
        for (unsigned i = 0; i < length(); ++i)
            m_digits[i] = digit(i);
#endif
    }

    explicit Int64BigIntImpl(uint64_t value)
        : m_value(value)
        , m_sign(false)
    {
#if CPU(REGISTER64)
        if (!isZero())
            m_digits[0] = digit(0);
#else
        for (unsigned i = 0; i < length(); ++i)
            m_digits[i] = digit(i);
#endif
    }

    ALWAYS_INLINE bool isZero() { return !m_value; }
    ALWAYS_INLINE bool sign() { return m_sign; }
    ALWAYS_INLINE unsigned length() { return isZero() ? 0 : numDigits; }
    ALWAYS_INLINE JSBigInt::Digit digit(unsigned i)
    {
        ASSERT_UNUSED(i, i < length());
#if CPU(REGISTER64)
        if (sign())
            return static_cast<JSBigInt::Digit>(WTF::negate(static_cast<int64_t>(m_value)));
        return m_value;
#else
        static_assert(sizeof(JSBigInt::Digit) == 4);
        if (sign())
            return static_cast<JSBigInt::Digit>(WTF::negate(static_cast<int64_t>(m_value)) >> (32 * i));
        return static_cast<JSBigInt::Digit>(m_value >> (32 * i));
#endif
    }

    ALWAYS_INLINE std::span<const JSBigInt::Digit> digits() { return { m_digits, length() }; }

private:
    friend struct JSBigInt::ImplResult;
    uint64_t m_value;
    JSBigInt::Digit m_digits[numDigits] { };
    bool m_sign;
};

ALWAYS_INLINE JSBigInt::ImplResult::ImplResult(HeapBigIntImpl& heapImpl)
    : payload(heapImpl.m_bigInt)
{ }

ALWAYS_INLINE JSBigInt::ImplResult::ImplResult(JSBigInt* heapBigInt)
    : payload(heapBigInt)
{ }

#if USE(BIGINT32)
ALWAYS_INLINE JSBigInt::ImplResult::ImplResult(Int32BigIntImpl& int32Impl)
    : payload(jsBigInt32(int32Impl.m_value))
{ }
#endif

ALWAYS_INLINE JSBigInt::ImplResult::ImplResult(JSValue value)
    : payload(value)
{ }

static ALWAYS_INLINE JSValue tryConvertToBigInt32(JSBigInt::ImplResult implResult)
{
    if (!implResult.payload)
        return JSValue();
    if (implResult.payload.isBigInt32())
        return implResult.payload;
    return tryConvertToBigInt32(implResult.payload.asHeapBigInt());
}

static ALWAYS_INLINE JSBigInt::ImplResult zeroImpl(JSGlobalObject* globalObject)
{
#if USE(BIGINT32)
    UNUSED_PARAM(globalObject);
    return jsBigInt32(0);
#else
    return JSBigInt::createZero(globalObject);
#endif
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::exponentiateImpl(JSGlobalObject* globalObject, BigIntImpl1 base, BigIntImpl2 exponent)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (exponent.sign()) {
        throwRangeError(globalObject, scope, "Negative exponent is not allowed"_s);
        return nullptr;
    }

    // 2. If base is 0n and exponent is 0n, return 1n.
    if (exponent.isZero())
        RELEASE_AND_RETURN(scope, JSBigInt::createFrom(globalObject, 1));
    
    // 3. Return a BigInt representing the mathematical value of base raised
    //    to the power exponent.
    if (base.isZero())
        return { base };

    if (base.length() == 1 && base.digit(0) == 1) {
        // (-1) ** even_number == 1.
        if (base.sign() && !(exponent.digit(0) & 1))
            RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, base));

        // (-1) ** odd_number == -1; 1 ** anything == 1.
        return { base };
    }

    // For all bases >= 2, very large exponents would lead to unrepresentable
    // results.
    static_assert(maxLengthBits < std::numeric_limits<Digit>::max(), "maxLengthBits needs to be less than digit::max()");
    if (exponent.length() > 1) {
        throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
        return nullptr;
    }

    Digit expValue = exponent.digit(0);
    if (expValue == 1)
        return { base };
    if (expValue >= maxLengthBits) {
        throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
        return nullptr;
    }

    static_assert(maxLengthBits <= maxInt, "maxLengthBits needs to be <= maxInt");
    int n = static_cast<int>(expValue);
    if (base.length() == 1 && base.digit(0) == 2) {
        // Fast path for 2^n.
        int neededDigits = 1 + (n / digitBits);

        Vector<Digit, 16> resultVector(neededDigits, 0);
        auto result = resultVector.mutableSpan();

        // All bits are zero. Now set the n-th bit.
        Digit msd = static_cast<Digit>(1) << (n % digitBits);
        result[neededDigits - 1] = msd;

        // Result is negative for odd powers of -2n.
        bool sign = false;
        if (base.sign()) 
            sign = static_cast<bool>(n & 1);
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, sign, result));
    }

    JSBigInt* result = nullptr;
    JSBigInt* runningSquare = base.toHeapBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // This implicitly sets the result's sign correctly.
    if (n & 1) {
        result = base.toHeapBigInt(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    n >>= 1;
    for (; n; n >>= 1) {
        ImplResult temp = JSBigInt::multiplyImpl(globalObject, HeapBigIntImpl { runningSquare }, HeapBigIntImpl { runningSquare });
        RETURN_IF_EXCEPTION(scope, nullptr);
        ASSERT(temp.payload);
        ASSERT(temp.payload.isHeapBigInt());
        JSBigInt* maybeResult = temp.payload.asHeapBigInt();
        runningSquare = maybeResult;
        if (n & 1) {
            if (!result)
                result = runningSquare;
            else {
                temp = JSBigInt::multiplyImpl(globalObject, HeapBigIntImpl { result }, HeapBigIntImpl { runningSquare });
                RETURN_IF_EXCEPTION(scope, nullptr);
                ASSERT(temp.payload);
                ASSERT(temp.payload.isHeapBigInt());
                maybeResult = temp.payload.asHeapBigInt();
                result = maybeResult;
            }
        }
    }

    return { result };
}

JSValue JSBigInt::exponentiate(JSGlobalObject* globalObject, JSBigInt* base, JSBigInt* exponent)
{
    return tryConvertToBigInt32(exponentiateImpl(globalObject, HeapBigIntImpl { base }, HeapBigIntImpl { exponent }));
}

#if USE(BIGINT32)
JSValue JSBigInt::exponentiate(JSGlobalObject* globalObject, JSBigInt* base, int32_t exponent)
{
    return tryConvertToBigInt32(exponentiateImpl(globalObject, HeapBigIntImpl { base }, Int32BigIntImpl { exponent }));
}

JSValue JSBigInt::exponentiate(JSGlobalObject* globalObject, int32_t base, JSBigInt* exponent)
{
    return tryConvertToBigInt32(exponentiateImpl(globalObject, Int32BigIntImpl { base }, HeapBigIntImpl { exponent }));
}

JSValue JSBigInt::exponentiate(JSGlobalObject* globalObject, int32_t base, int32_t exponent)
{
    return tryConvertToBigInt32(exponentiateImpl(globalObject, Int32BigIntImpl { base }, Int32BigIntImpl { exponent }));
}
#endif

#if USE(JSVALUE32_64)
using TwoDigit = uint64_t;
#else
using TwoDigit = UInt128;
#endif

template<size_t N>
class CombaAccumulator {
    using Digit = JSBigInt::Digit;
public:
    ALWAYS_INLINE void mac(Digit a, Digit b)
    {
        TwoDigit prod = static_cast<TwoDigit>(a) * b;
        TwoDigit sum0 = static_cast<TwoDigit>(t0) + static_cast<Digit>(prod);
        t0 = static_cast<Digit>(sum0);
        TwoDigit sum1 = static_cast<TwoDigit>(t1) + static_cast<Digit>(prod >> JSBigInt::digitBits) + static_cast<Digit>(sum0 >> JSBigInt::digitBits);
        t1 = static_cast<Digit>(sum1);
        t2 += static_cast<Digit>(sum1 >> JSBigInt::digitBits);
    }

    ALWAYS_INLINE Digit storeAndShift()
    {
        Digit result = t0;
        t0 = t1;
        t1 = t2;
        t2 = 0;
        return result;
    }

    template<size_t K, size_t I = 0>
    ALWAYS_INLINE void computeColumn(std::span<const Digit, N> a, std::span<const Digit, N> b)
    {
        if constexpr (I < N) {
            constexpr int J = static_cast<int>(K) - static_cast<int>(I);
            if constexpr (J >= 0 && J < static_cast<int>(N))
                mac(a[I], b[J]);
            computeColumn<K, I + 1>(a, b);
        }
    }

    template<size_t K = 0>
    ALWAYS_INLINE void pass(std::span<Digit, N * 2> r, std::span<const Digit, N> a, std::span<const Digit, N> b)
    {
        if constexpr (K < 2 * N - 1) {
            computeColumn<K>(a, b);
            r[K] = storeAndShift();
            pass<K + 1>(r, a, b);
        } else
            r[N * 2 - 1] = t0;
    }

private:
    Digit t0 { 0 };
    Digit t1 { 0 };
    Digit t2 { 0 };
};

template<size_t N>
std::span<JSBigInt::Digit, N * 2> JSBigInt::multiplyComba(std::span<const Digit, N> x, std::span<const Digit, N> y, std::span<Digit, N * 2> result)
{
    static_assert(N == 1 || N == 2 || N == 4 || N == 8 || N == 16);
    std::array<Digit, N> a;
    std::array<Digit, N> b;

    // Ensure that all loads are done before entering to computation. This allows compiler to use registers for all elements.
    for (size_t i = 0; i < N; ++i)
        a[i] = x[i];
    for (size_t i = 0; i < N; ++i)
        b[i] = y[i];

    CombaAccumulator<N> acc;
    acc.template pass<>(result, a, b);
    return result;
}

std::span<JSBigInt::Digit> JSBigInt::multiplySingle(std::span<const Digit> multiplicand, Digit multiplier, std::span<Digit> result)
{
    RELEASE_ASSERT(result.size() > multiplicand.size());
    Digit carry = 0;
    Digit high = 0;
    size_t i = 0;
    for (; i < multiplicand.size(); ++i) {
        auto [low, newHigh] = digitMul(multiplicand[i], multiplier);
        Digit newCarry = 0;
        result[i] = digitAdd3(low, high, carry, newCarry);
        high = newHigh;
        carry = newCarry;
    }
    result[i++] = carry + high;
    return result.first(i);
}

// Z := X * Y.
// O(n²) "schoolbook" multiplication algorithm. Optimized to minimize
// bounds and overflow checks: rather than looping over X for every digit
// of Y (or vice versa), we loop over Z. The {BODY} macro above is what
// computes one of Z's digits as a sum of the products of relevant digits
// of X and Y. This yields a nearly 2x improvement compared to more obvious
// implementations.
// This method is *highly* performance sensitive even for the advanced
// algorithms, which use this as the base case of their recursive calls.
std::span<JSBigInt::Digit> JSBigInt::multiplyTextbook(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
#define BODY(min, max) \
    do { \
        for (uint32_t j = min; j <= max; j++) { \
            auto [low, high] = digitMul(x[j], y[i - j]); \
            zi = digitAdd(zi, low, carry); \
            next = digitAdd(next, high, nextCarry); \
        } \
        result[i] = zi; \
    } while (0)

    ASSERT(x.size() >= y.size());
    ASSERT(result.size() >= x.size() + y.size());
    ASSERT(x.size());
    ASSERT(y.size());

    Digit next = 0, nextCarry = 0, carry = 0;
    // Unrolled first iteration: it's trivial.
    {
        auto [low, high] = digitMul(x[0], y[0]);
        result[0] = low;
        next = high;
    }
    size_t i = 1;
    // Unrolled second iteration: a little less setup.
    if (i < y.size()) {
        Digit zi = next;
        next = 0;
        BODY(0, 1);
        i++;
    }

    // Main part: since x.size() >= y.size() > i, no bounds checks are needed.
    for (; i < y.size(); i++) {
        Digit temp = 0;
        Digit zi = digitAdd(next, carry, temp);
        next = nextCarry + temp;
        carry = 0;
        nextCarry = 0;
        BODY(0, i);
    }

    // Last part: i exceeds y now, we have to be careful about bounds.
    size_t loopEnd = x.size() + y.size() - 2;
    for (; i <= loopEnd; i++) {
        size_t maxXIndex = std::min<size_t>(i, x.size() - 1);
        size_t maxYIndex = y.size() - 1;
        size_t minXIndex = i - maxYIndex;
        Digit temp = 0;
        Digit zi = digitAdd(next, carry, temp);
        next = nextCarry + temp;
        carry = 0;
        nextCarry = 0;
        BODY(minXIndex, maxXIndex);
    }

    // Write the last digit.
    Digit temp = 0;
    result[i++] = digitAdd(next, carry, temp);
    ASSERT(!temp);
    return result.first(i);
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::multiplyImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.length() < y.length())
        RELEASE_AND_RETURN(scope, multiplyImpl(globalObject, y, x));

    ASSERT(x.length() >= y.length());

    if (y.isZero())
        return { y };

    unsigned resultLength = x.length() + y.length();
    bool resultSign = x.sign() != y.sign();

    if (y.length() == 1) {
        if (y.digit(0) == 1) {
            if (resultSign == x.sign())
                return { x };
            RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, x));
        }
    }

    Vector<Digit, 32> digits(resultLength);
    auto span = digits.mutableSpan();
    std::span<Digit> result = ([](std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> span) -> std::span<Digit> {
        if (x.size() == y.size()) {
            switch (y.size()) {
            case 1:
                return multiplyComba<1>(x.template first<1>(), y.template first<1>(), span.first<2>());
            case 2:
                return multiplyComba<2>(x.template first<2>(), y.template first<2>(), span.first<4>());
            case 4:
                return multiplyComba<4>(x.template first<4>(), y.template first<4>(), span.first<8>());
            case 8:
                return multiplyComba<8>(x.template first<8>(), y.template first<8>(), span.first<16>());
            case 16:
                return multiplyComba<16>(x.template first<16>(), y.template first<16>(), span.first<32>());
            }
        }
        if (y.size() == 1)
            return multiplySingle(x, y[0], span);
        return multiplyTextbook(x, y, span);
    }(x.digits(), y.digits(), span));
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, resultSign, result));
}

JSValue JSBigInt::multiply(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(multiplyImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::multiply(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(multiplyImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
JSValue JSBigInt::multiply(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(multiplyImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
#endif

// The algorithm is from "Division by Invariant Integers using Multiplication", Granlund and Montgomery, PLDI'94.
// https://gmplib.org/~tege/divcnst-pldi94.pdf
// This is summarized in "Improved division by invariant integers", Moller and Granlund, as "Previous Methods".
// https://gmplib.org/~tege/division-paper.pdf
//
// We implemented both previous and new methods and it turned out that the previous method is faster than the new method.
// The reason is ARM64 etc. has umulhi, which performs high umul in a extremely fast manner than the older hardware.
// So making the following adjustment branch more predictable is profitable than avoiding umulhi.
class DigitDiv {
    using Digit = JSBigInt::Digit;
    static constexpr auto digitBits = JSBigInt::digitBits;
public:
    DigitDiv(Digit d)
        : m_divisor(d)
        , m_inverse(calculateInverse(d))
    {
    }

    ALWAYS_INLINE Digit div(Digit high, Digit low, Digit& remainder)
    {
        ASSERT(high < m_divisor); // This means that quotient is within Digit. This is an invariant used in digitDiv too.
        Digit u1 = high;
        Digit u0 = low;
        Digit v = m_inverse;
        Digit d = m_divisor;

        // 1. q = ((v * u1) / beta) + u1
        Digit q = u1 + static_cast<Digit>((static_cast<TwoDigit>(u1) * static_cast<TwoDigit>(v)) >> digitBits);

        // 2. <p1, p0> = q * d
        TwoDigit p = static_cast<TwoDigit>(q) * static_cast<TwoDigit>(d);

        // 3. <r1, r0> = <u1, u0> - <p1, p0>
        TwoDigit u = (static_cast<TwoDigit>(u1) << digitBits) | u0;
        TwoDigit rem = u - p;

        // 4. while (r1 > 0 || r0 >= d) { q++; <r1, r0> = <r1, r0> - d; }
        while (rem >= d) {
            q++;
            rem -= d;
        }

        Digit r = static_cast<Digit>(rem);

#if ASSERT_ENABLED
        Digit refR = 0;
        Digit refQ = JSBigInt::digitDiv(u1, u0, d, refR);
        if (!(refR == r && refQ == q)) [[unlikely]] {
            dataLogLn(u1, " ", u0, " ", d, " ", refR, " ", r, " ", refR == r, " ", refQ, " ", q, " ", refQ == q);
            ASSERT(refR == r);
            ASSERT(refQ == q);
        }
#endif

        remainder = r;
        return q;
    }

private:
    static ALWAYS_INLINE Digit calculateInverse(Digit d)
    {
        ASSERT(d & (1ULL << (digitBits - 1))); // d is already normalized.
        TwoDigit limit = ~static_cast<TwoDigit>(0);
        return static_cast<Digit>(limit / d);
    }

    const Digit m_divisor { };
    const Digit m_inverse { };
};

// Computes Q(uotient) and remainder for A/b, such that
// Q = (A - remainder) / b, with 0 <= remainder < b.
// If Q.len == 0, only the remainder will be returned.
// Q may be the same as A for an in-place division.
std::span<JSBigInt::Digit> JSBigInt::divideSingle(std::span<Digit> q, Digit& remainder, std::span<const Digit> a, Digit b)
{
    RELEASE_ASSERT(b != 0);
    RELEASE_ASSERT(a.size() > 0);
    remainder = 0;
    size_t length = a.size();
    if (!q.empty()) {
        if (a[length - 1] >= b) {
            RELEASE_ASSERT(q.size() >= a.size());
            for (size_t i = length; i-- > 0;)
                q[i] = digitDiv(remainder, a[i], b, remainder);
            return q.first(length);
        }

        RELEASE_ASSERT(q.size() >= a.size() - 1);
        remainder = a[length - 1];
        for (size_t i = length - 1; i-- > 0;)
            q[i] = digitDiv(remainder, a[i], b, remainder);
        return q.first(length - 1);
    }

    for (size_t i = length; i-- > 0;)
        digitDiv(remainder, a[i], b, remainder);
    return { };
}

JSBigInt::Digit JSBigInt::addAndReturnCarry(std::span<Digit> z, std::span<const Digit> x, std::span<const Digit> y)
{
    RELEASE_ASSERT(z.size() >= y.size() && x.size() >= y.size());
    Digit carry = 0;
    for (size_t i = 0; i < y.size(); i++) {
        Digit newCarry = 0;
        z[i] = digitAdd3(x[i], y[i], carry, newCarry);
        carry = newCarry;
    }
    return carry;
}

JSBigInt::Digit JSBigInt::subtractAndReturnBorrow(std::span<Digit> z, std::span<const Digit> x, std::span<const Digit> y)
{
    RELEASE_ASSERT(z.size() >= y.size() && x.size() >= y.size());
    Digit borrow = 0;
    for (size_t i = 0; i < y.size(); i++) {
        Digit borrowOut = 0;
        z[i] = digitSub2(x[i], y[i], borrow, borrowOut);
        borrow = borrowOut;
    }
    return borrow;
}

// Z += X. Returns the "carry" (0 or 1) after adding all of X's digits.
JSBigInt::Digit JSBigInt::inplaceAdd(std::span<Digit> z, std::span<const Digit> x)
{
    return addAndReturnCarry(z, z, x);
}

// Z -= X. Returns the "borrow" (0 or 1) after subtracting all of X's digits.
JSBigInt::Digit JSBigInt::inplaceSub(std::span<Digit> z, std::span<const Digit> x)
{
  return subtractAndReturnBorrow(z, z, x);
}

static std::span<JSBigInt::Digit> spanCopy(std::span<JSBigInt::Digit> z, std::span<const JSBigInt::Digit> x)
{
    if (z.data() == x.data())
        return z;
    memmoveSpan(z, x);
    return z.first(x.size());
}

// Z := X << shift
// Z and X may alias for an in-place shift.
std::span<JSBigInt::Digit> JSBigInt::leftShift(std::span<Digit> z, std::span<const Digit> x, unsigned shift)
{
    ASSERT(shift < digitBits);
    ASSERT(z.size() >= x.size());
    if (shift == 0)
        return spanCopy(z, x);

    Digit carry = 0;
    size_t i = 0;
    for (; i < x.size(); i++) {
        Digit d = x[i];
        z[i] = (d << shift) | carry;
        carry = d >> (digitBits - shift);
    }

    if (i < z.size())
        z[i++] = carry;
    else {
        ASSERT(carry == 0);
    }
    return z.first(i);
}

// Z := X >> shift
// Z and X may alias for an in-place shift.
std::span<JSBigInt::Digit> JSBigInt::rightShift(std::span<Digit> z, std::span<const Digit> x, unsigned shift)
{
    ASSERT(shift < digitBits);
    x = normalize(x);
    if (shift == 0)
        return spanCopy(z, x);

    if (x.empty())
        return { };

    RELEASE_ASSERT(z.size() >= x.size());
    Digit carry = x[0] >> shift;
    size_t last = x.size() - 1;
    size_t i = 0;
    for (; i < last; i++) {
        Digit d = x[i + 1];
        z[i] = (d << (digitBits - shift)) | carry;
        carry = d >> shift;
    }
    z[i++] = carry;
    return z.first(x.size());
}

// Computes Q(uotient) and R(emainder) for A/B, such that
// Q = (A - R) / B, with 0 <= R < B.
// Both Q and R are optional: callers that are only interested in one of them
// can pass the other with len == 0.
// If Q is present, its length must be at least A.len - B.len + 1.
// If R is present, its length must be at least B.len.
// See Knuth, Volume 2, section 4.3.1, Algorithm D.
std::tuple<std::span<JSBigInt::Digit>, std::span<JSBigInt::Digit>> JSBigInt::divideTextbook(std::span<Digit> q, std::span<Digit> r, std::span<const Digit> a, std::span<const Digit> b)
{
    RELEASE_ASSERT(b.size() >= 2); // Use divideSingle otherwise.
    RELEASE_ASSERT(a.size() >= b.size()); // No-op otherwise.
    RELEASE_ASSERT(q.empty() || q.size() >= a.size() - b.size() + 1);
    RELEASE_ASSERT(r.empty() || r.size() >= b.size());

    // The unusual variable names inside this function are consistent with
    // Knuth's book, as well as with Go's implementation of this algorithm.
    // Maintaining this consistency is probably more useful than trying to
    // come up with more descriptive names for them.
    const size_t n = b.size();
    const size_t m = a.size() - n;

    // D1.
    // Left-shift inputs so that the divisor's MSB is set. This is necessary
    // to prevent the digit-wise divisions (see digitDiv call below) from
    // overflowing (they take a two digits wide input, and return a one digit
    // result).
    Digit lastDigit = b[n - 1];
    unsigned shift = clz(lastDigit);

    // Allocate divisor storage and normalize if needed.
    Vector<Digit, 16> normalizedDivisorStorage;
    std::span<const Digit> normalizedDivisor;
    if (shift > 0) {
        normalizedDivisorStorage.resize(n);
        auto divisorSpan = normalizedDivisorStorage.mutableSpan();
        auto filled = leftShift(divisorSpan, b, shift);
        ASSERT_UNUSED(filled, filled.size() == divisorSpan.size());
        normalizedDivisor = divisorSpan;
    } else
        normalizedDivisor = b;
    RELEASE_ASSERT(normalizedDivisor.size() == b.size());

    // U holds the (continuously updated) remaining part of the dividend, which
    // eventually becomes the remainder.
    Vector<Digit, 16> u(a.size() + 1);
    auto uSpan = u.mutableSpan();
    {
        auto filled = leftShift(uSpan, a, shift);
        if (uSpan.size() != filled.size())
            zeroSpan(uSpan.subspan(filled.size()));
    }
    RELEASE_ASSERT(uSpan.size() == a.size() + 1);

    // In each iteration, {qhatv} holds {divisor} * {current quotient digit}.
    // "v" is the book's name for {divisor}, "qhat" the current quotient digit.
    Vector<Digit, 16> qhatv(n + 1);
    auto qhatvSpan = qhatv.mutableSpan();
    RELEASE_ASSERT(qhatvSpan.size() == n + 1);

    // D2.
    // Iterate over the dividend's digits (like the "grad school" algorithm).
    // {vn1} is the divisor's most significant digit.
    // Since {n} is >= 2, {vn1} and {vn2} are always accessible.
    Digit vn1 = normalizedDivisor[n - 1];
    Digit vn2 = normalizedDivisor[n - 2];
    DigitDiv digitDiv(vn1);
    for (size_t j = m + 1; j-- > 0;) {
        // D3.
        // Estimate the current iteration's quotient digit (see Knuth for details).
        // {qhat} is the current quotient digit.
        Digit qhat = std::numeric_limits<Digit>::max();

        // {ujn} is the dividend's most significant remaining digit.
        Digit ujn = uSpan[j + n];
        if (ujn != vn1) {
            // {rhat} is the current iteration's remainder.
            Digit rhat = 0;
            // Estimate the current quotient digit by dividing the most significant
            // digits of dividend and divisor. The result will not be too small,
            // but could be a bit too large.
            qhat = digitDiv.div(ujn, uSpan[j + n - 1], rhat);

            // Decrement the quotient estimate as needed by looking at the next
            // digit, i.e. by testing whether
            // qhat * v_{n-2} > (rhat << digitBits) + u_{j+n-2}.
            Digit ujn2 = uSpan[j + n - 2];
            while (productGreaterThan(qhat, vn2, rhat, ujn2)) {
                qhat--;
                Digit prevRhat = rhat;
                rhat += vn1;
                // v[n-1] >= 0, so this tests for overflow.
                if (rhat < prevRhat)
                    break;
            }
        }

        // D4.
        // Multiply the divisor with the current quotient digit, and subtract
        // it from the dividend. If there was "borrow", then the quotient digit
        // was one too high, so we must correct it and undo one subtraction of
        // the (shifted) divisor.
        if (qhat != 0) {
            auto filled = multiplySingle(normalizedDivisor, qhat, qhatvSpan);
            if (qhatvSpan.size() != filled.size())
                zeroSpan(qhatvSpan.subspan(filled.size()));

            Digit c = inplaceSub(uSpan.subspan(j), qhatvSpan);
            if (c) {
                c = inplaceAdd(uSpan.subspan(j), normalizedDivisor);
                uSpan[j + n] = uSpan[j + n] + c;
                qhat--;
            }
        }

        if (!q.empty()) {
            if (j >= q.size())
                RELEASE_ASSERT(qhat == 0);
            else
                q[j] = qhat;
        }
    }

    // Determine the actual quotient length: it's m+1 if q[m] is non-zero, otherwise m.
    auto qResult = q;
    if (!q.empty())
        qResult = q.first(m + 1);
    auto rResult = r;
    if (!r.empty())
        rResult = rightShift(r, uSpan, shift);

    return { qResult, rResult };
}

static ALWAYS_INLINE JSBigInt::Digit estimateQhat(std::span<const JSBigInt::Digit> a, std::span<const JSBigInt::Digit> b)
{
    ASSERT(a.size() == b.size());
    const size_t n = a.size();
    ASSERT(n > 1); // one digit case is already handled via divideSingle.
    ASSERT(b.back() != 0); // b should be normalized
    ASSERT(a.back() > b.back());

    constexpr auto digitBits = JSBigInt::digitBits;

    // a.back() > b.back(), so a > b and quotient is at least 1.
    // Since a and b have the same number of digits, the quotient fits in one digit.
    //
    // We estimate qhat from Algorithm D by normalizing only the top 2-3 digits (no vector allocation),
    // then verify by computing qhat * b and comparing with a.

    unsigned shift = clz(b.back());

    // Compute normalized top digits inline.
    // vn1 = normalized b[n-1]
    // vn2 = normalized b[n-2]
    // un = overflow from normalizing a (high digit after left shift)
    // un1 = normalized a[n-1]
    // un2 = normalized a[n-2]
    JSBigInt::Digit vn1, vn2, un, un1, un2;

    if (shift == 0) {
        vn1 = b[n - 1];
        vn2 = b[n - 2];
        un = 0;
        un1 = a[n - 1];
        un2 = a[n - 2];
    } else {
        // Left-shift by 'shift' bits to normalize
        vn1 = (b[n - 1] << shift) | (b[n - 2] >> (digitBits - shift));
        vn2 = (b[n - 2] << shift) | (n >= 3 ? (b[n - 3] >> (digitBits - shift)) : 0);
        un = a[n - 1] >> (digitBits - shift);
        un1 = (a[n - 1] << shift) | (a[n - 2] >> (digitBits - shift));
        un2 = (a[n - 2] << shift) | (n >= 3 ? (a[n - 3] >> (digitBits - shift)) : 0);
    }

    // Estimate quotient using the normalized top digits.
    // Since a and b have the same number of digits with a.back() > b.back(),
    // after normalization un < vn1 is guaranteed:
    // - vn1 has its MSB set, so vn1 >= 2^(digitBits-1)
    // - un = a[n-1] >> (digitBits - shift), which is at most 2^(digitBits-1) - 1
    ASSERT(un < vn1);

    JSBigInt::Digit rhat = 0;
    JSBigInt::Digit qhat = JSBigInt::digitDiv(un, un1, vn1, rhat);

    // Refine qhat using the second most significant digit of divisor.
    while (JSBigInt::productGreaterThan(qhat, vn2, rhat, un2)) {
        qhat--;
        JSBigInt::Digit prevRhat = rhat;
        rhat += vn1;
        if (rhat < prevRhat)
            break;
    }

    return qhat;
}

JSBigInt::Digit JSBigInt::divideSameSize(std::span<const Digit> a, std::span<const Digit> b)
{
    RELEASE_ASSERT(a.size() == b.size());
    const size_t n = a.size();
    RELEASE_ASSERT(n >= 2); // Use divideSingle otherwise.
    ASSERT(b.back() != 0); // b should be normalized

    // absoluteCompare ensures that a > b.

    // MSB is the same. Thus result is 1 or 0.
    if (a.back() == b.back())
        return 1; // a >= b

    Digit qhat = estimateQhat(a, b);

    // Verify qhat by computing qhat * b and comparing with a inline.
    // If qhat * b > a, decrement qhat.
    Digit mulCarry = 0;
    Digit subBorrow = 0;
    for (size_t i = 0; i < n; i++) {
        // Compute qhat * b[i] + mulCarry
        auto [low, high] = digitMul(qhat, b[i]);
        Digit product = low + mulCarry;
        mulCarry = high + (product < low ? 1 : 0);

        // Subtract product from a[i] to check if qhat * b > a
        Digit borrowOut = 0;
        digitSub2(a[i], product, subBorrow, borrowOut);
        subBorrow = borrowOut;
    }

    // If there's overflow from multiplication or borrow from subtraction,
    // qhat * b > a, so decrement qhat.
    if (mulCarry || subBorrow)
        qhat--;

    return qhat;
}

std::span<JSBigInt::Digit> JSBigInt::remainderSameSize(std::span<Digit> r, std::span<const Digit> a, std::span<const Digit> b)
{
    RELEASE_ASSERT(a.size() == b.size());
    const size_t n = a.size();
    RELEASE_ASSERT(n >= 2); // Use divideSingle otherwise.
    ASSERT(b.back() != 0); // b should be normalized
    ASSERT(r.size() >= n);

    // absoluteCompare ensures that a > b.

    // a.back() == b.back(): quotient is 0 or 1
    if (a.back() == b.back())
        return subTextbook(a, b, r);

    Digit qhat = estimateQhat(a, b);

    // Compute remainder = a - qhat * b inline without allocating a vector.
    // We compute qhat * b and subtract from a in a single pass.
    Digit mulCarry = 0;
    Digit subBorrow = 0;
    for (size_t i = 0; i < n; i++) {
        // Compute qhat * b[i] + mulCarry
        auto [low, high] = digitMul(qhat, b[i]);
        Digit product = low + mulCarry;
        mulCarry = high + (product < low ? 1 : 0);

        // Compute r[i] = a[i] - product - subBorrow
        Digit borrowOut = 0;
        r[i] = digitSub2(a[i], product, subBorrow, borrowOut);
        subBorrow = borrowOut;
    }

    // If there's overflow from multiplication or borrow from subtraction,
    // qhat was too large, add back b.
    if (mulCarry || subBorrow)
        inplaceAdd(r.first(n), b);

    return r.first(n);
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::divideImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    // 1. If y is 0n, throw a RangeError exception.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (y.isZero()) {
        throwRangeError(globalObject, scope, "0 is an invalid divisor value."_s);
        return nullptr;
    }

    // 2. Let quotient be the mathematical value of x divided by y.
    // 3. Return a BigInt representing quotient rounded towards 0 to the next
    //    integral value.
    bool resultSign = x.sign() != y.sign();
    switch (absoluteCompare(x, y)) {
    case ComparisonResult::LessThan: {
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));
    }
    case ComparisonResult::Equal: {
        RELEASE_AND_RETURN(scope, createFrom(globalObject, vm, resultSign ? -1 : 1));
    }
    case ComparisonResult::GreaterThan:
    case ComparisonResult::Undefined:
        break;
    }

    auto xSpan = x.digits();
    auto ySpan = y.digits();
    size_t qLength = xSpan.size() - ySpan.size() + 1;
    if (ySpan.size() == 1) {
        Digit divisor = ySpan[0];
        if (divisor == 1) {
            if (resultSign == x.sign())
                return JSBigInt::ImplResult { x };
            RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, x));
        }

        Vector<Digit, 16> q(qLength);
        Digit remainder;
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, resultSign, divideSingle(q.mutableSpan(), remainder, xSpan, divisor)));
    }

    if (xSpan.size() == ySpan.size()) {
        auto quotientDigit = divideSameSize(xSpan, ySpan);
        if (!quotientDigit)
            RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

        auto* quotient = createWithLength(globalObject, 1);
        RETURN_IF_EXCEPTION(scope, nullptr);

        quotient->setDigit(0, quotientDigit);
        quotient->setSign(resultSign);
        return quotient;
    }

    Vector<Digit, 16> q(qLength);
    auto [qSpan, rSpan] = divideTextbook(q.mutableSpan(), { }, xSpan, ySpan);
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, resultSign, qSpan));
}

JSValue JSBigInt::divide(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(divideImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::divide(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(divideImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::divide(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(divideImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl>
JSBigInt* JSBigInt::copy(JSGlobalObject* globalObject, BigIntImpl x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!x.isZero());

    JSBigInt* result = createWithLength(globalObject, x.length());
    RETURN_IF_EXCEPTION(scope, nullptr);
    memcpySpan(result->digits(), x.digits());
    result->setSign(x.sign());
    return result;
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::unaryMinusImpl(JSGlobalObject* globalObject, BigIntImpl x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.isZero())
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    JSBigInt* result = copy(globalObject, x);
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->setSign(!x.sign());
    return result;
}

JSValue JSBigInt::unaryMinus(JSGlobalObject* globalObject, JSBigInt* x)
{
    return tryConvertToBigInt32(unaryMinusImpl(globalObject, HeapBigIntImpl { x }));
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::remainderImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    // 1. If y is 0n, throw a RangeError exception.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (y.isZero()) {
        throwRangeError(globalObject, scope, "0 is an invalid divisor value."_s);
        return nullptr;
    }

    // 2. Return the JSBigInt representing x modulo y.
    // See https://github.com/tc39/proposal-bigint/issues/84 though.
    switch (absoluteCompare(x, y)) {
    case ComparisonResult::LessThan: {
        return { x };
    }
    case ComparisonResult::Equal: {
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));
    }
    case ComparisonResult::GreaterThan:
    case ComparisonResult::Undefined:
        break;
    }

    auto xSpan = x.digits();
    auto ySpan = y.digits();
    if (ySpan.size() == 1) {
        Digit divisor = ySpan[0];
        if (divisor == 1)
            RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

        Digit remainderDigit;
        divideSingle({ }, remainderDigit, xSpan, divisor);
        if (!remainderDigit)
            RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

        auto* remainder = createWithLength(globalObject, 1);
        RETURN_IF_EXCEPTION(scope, nullptr);

        remainder->setDigit(0, remainderDigit);
        remainder->setSign(x.sign());
        return remainder;
    }

    Vector<Digit, 16> r(ySpan.size());
    std::span<const Digit> rSpan;
    if (xSpan.size() == ySpan.size())
        rSpan = remainderSameSize(r.mutableSpan(), xSpan, ySpan);
    else
        rSpan = std::get<1>(divideTextbook({ }, r.mutableSpan(), xSpan, ySpan));
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, x.sign(), rSpan));
}

JSValue JSBigInt::remainder(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(remainderImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::remainder(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(remainderImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::remainder(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(remainderImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::incImpl(JSGlobalObject* globalObject, BigIntImpl x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto xSpan = x.digits();
    if (!x.sign()) {
        Vector<Digit, 16> resultVector(addOneLength(xSpan));
        auto result = absoluteAddOne(xSpan, resultVector.mutableSpan());
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, result));
    }

    Vector<Digit, 16> resultVector(subOneLength(xSpan));
    auto result = absoluteSubOne(xSpan, resultVector.mutableSpan());
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, true, result));
}

JSValue JSBigInt::inc(JSGlobalObject* globalObject, JSBigInt* x)
{
    return tryConvertToBigInt32(incImpl(globalObject, HeapBigIntImpl { x }));
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::decImpl(JSGlobalObject* globalObject, BigIntImpl x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.isZero()) {
#if USE(BIGINT32)
        return jsBigInt32(-1);
#else
        RELEASE_AND_RETURN(scope, createFrom(globalObject, -1));
#endif
    }

    auto xSpan = x.digits();
    if (!x.sign()) {
        Vector<Digit, 16> resultVector(subOneLength(xSpan));
        auto result = absoluteSubOne(xSpan, resultVector.mutableSpan());
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, result));
    }

    Vector<Digit, 16> resultVector(addOneLength(xSpan));
    auto result = absoluteAddOne(xSpan, resultVector.mutableSpan());
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, true, result));
}

JSValue JSBigInt::dec(JSGlobalObject* globalObject, JSBigInt* x)
{
    return tryConvertToBigInt32(decImpl(globalObject, HeapBigIntImpl { x }));
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::addImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    bool xSign = x.sign();

    // x + y == x + y
    // -x + -y == -(x + y)
    if (xSign == y.sign())
        return absoluteAdd(globalObject, x, y, xSign);

    // x + -y == x - y == -(y - x)
    // -x + y == y - x == -(x - y)
    ComparisonResult comparisonResult = absoluteCompare(x, y);
    if (comparisonResult == ComparisonResult::GreaterThan || comparisonResult == ComparisonResult::Equal)
        return absoluteSub(globalObject, x, y, xSign);

    return absoluteSub(globalObject, y, x, !xSign);
}
JSValue JSBigInt::add(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(addImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::add(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(addImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::add(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(addImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::subImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    bool xSign = x.sign();
    if (xSign != y.sign()) {
        // x - (-y) == x + y
        // (-x) - y == -(x + y)
        return absoluteAdd(globalObject, x, y, xSign);
    }
    // x - y == -(y - x)
    // (-x) - (-y) == y - x == -(x - y)
    ComparisonResult comparisonResult = absoluteCompare(x, y);
    if (comparisonResult == ComparisonResult::GreaterThan || comparisonResult == ComparisonResult::Equal)
        return absoluteSub(globalObject, x, y, xSign);

    return absoluteSub(globalObject, y, x, !xSign);
}

JSValue JSBigInt::sub(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(subImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::sub(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(subImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::sub(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(subImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::bitwiseAndImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto xSpan = x.digits();
    auto ySpan = y.digits();
    if (!x.sign() && !y.sign()) {
        Vector<Digit, 16> resultVector(andLength(xSpan, ySpan));
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, absoluteAnd(xSpan, ySpan, resultVector.mutableSpan())));
    }

    if (x.sign() && y.sign()) {
        // (-x) & (-y) == ~(x-1) & ~(y-1) == ~((x-1) | (y-1))
        // == -(((x-1) | (y-1)) + 1)
        Vector<Digit, 16> resultXVector(subOneLength(xSpan));
        auto resultX = normalize(absoluteSubOne(xSpan, resultXVector.mutableSpan()));

        Vector<Digit, 16> resultYVector(subOneLength(ySpan));
        auto resultY = normalize(absoluteSubOne(ySpan, resultYVector.mutableSpan()));

        Vector<Digit, 16> resultVector(orLength(resultX, resultY));
        auto result = normalize(absoluteOr(resultX, resultY, resultVector.mutableSpan()));

        Vector<Digit, 16> finalResultVector(addOneLength(result));
        auto finalResult = absoluteAddOne(result, finalResultVector.mutableSpan());

        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, true, finalResult));
    }

    ASSERT(x.sign() != y.sign());
    // x & (-y) == x & ~(y-1)
    auto computeResult = [&] (auto x, auto y) -> JSBigInt* {
        ASSERT(!x.sign()); 
        ASSERT(y.sign()); 
        auto xSpan = x.digits();
        auto ySpan = y.digits();
        Vector<Digit, 16> resultYVector(subOneLength(ySpan));
        auto resultY = normalize(absoluteSubOne(ySpan, resultYVector.mutableSpan()));

        Vector<Digit, 16> resultVector(andNotLength(xSpan, resultY));
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, absoluteAndNot(xSpan, resultY, resultVector.mutableSpan())));
    };
    if (x.sign())
        return computeResult(y, x);
    return computeResult(x, y);
}

JSValue JSBigInt::bitwiseAnd(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(bitwiseAndImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::bitwiseAnd(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(bitwiseAndImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::bitwiseAnd(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(bitwiseAndImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::bitwiseOrImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto xSpan = x.digits();
    auto ySpan = y.digits();
    if (!x.sign() && !y.sign()) {
        Vector<Digit, 16> resultVector(orLength(xSpan, ySpan));
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, absoluteOr(xSpan, ySpan, resultVector.mutableSpan())));
    }
    
    if (x.sign() && y.sign()) {
        // (-x) | (-y) == ~(x-1) | ~(y-1) == ~((x-1) & (y-1))
        // == -(((x-1) & (y-1)) + 1)
        Vector<Digit, 16> resultXVector(subOneLength(xSpan));
        auto resultX = normalize(absoluteSubOne(xSpan, resultXVector.mutableSpan()));

        Vector<Digit, 16> resultYVector(subOneLength(ySpan));
        auto resultY = normalize(absoluteSubOne(ySpan, resultYVector.mutableSpan()));

        Vector<Digit, 16> resultVector(andLength(resultX, resultY));
        auto result = normalize(absoluteAnd(resultX, resultY, resultVector.mutableSpan()));

        Vector<Digit, 16> finalResultVector(addOneLength(result));
        auto finalResult = absoluteAddOne(result, finalResultVector.mutableSpan());

        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, true, finalResult));
    }

    ASSERT(x.sign() != y.sign());

    // x | (-y) == x | ~(y-1) == ~((y-1) &~ x) == -(((y-1) &~ x) + 1)
    auto computeResult = [&] (auto x, auto y) -> JSBigInt* {
        ASSERT(!x.sign());
        ASSERT(y.sign());

        auto xSpan = x.digits();
        auto ySpan = y.digits();
        Vector<Digit, 16> resultYVector(subOneLength(ySpan));
        auto resultY = normalize(absoluteSubOne(ySpan, resultYVector.mutableSpan()));

        Vector<Digit, 16> resultVector(andNotLength(resultY, xSpan));
        auto result = normalize(absoluteAndNot(resultY, xSpan, resultVector.mutableSpan()));

        Vector<Digit, 16> finalResultVector(addOneLength(result));
        auto finalResult = absoluteAddOne(result, finalResultVector.mutableSpan());

        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, true, finalResult));
    };

    if (x.sign())
        return computeResult(y, x);
    return computeResult(x, y);
}

JSValue JSBigInt::bitwiseOr(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(bitwiseOrImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::bitwiseOr(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(bitwiseOrImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::bitwiseOr(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(bitwiseOrImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::bitwiseXorImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto xSpan = x.digits();
    auto ySpan = y.digits();
    if (!x.sign() && !y.sign()) {
        Vector<Digit, 16> resultVector(xorLength(xSpan, ySpan));
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, absoluteXor(xSpan, ySpan, resultVector.mutableSpan())));
    }

    if (x.sign() && y.sign()) {
        // (-x) ^ (-y) == ~(x-1) ^ ~(y-1) == (x-1) ^ (y-1)
        Vector<Digit, 16> resultXVector(subOneLength(xSpan));
        auto resultX = normalize(absoluteSubOne(xSpan, resultXVector.mutableSpan()));

        Vector<Digit, 16> resultYVector(subOneLength(ySpan));
        auto resultY = normalize(absoluteSubOne(ySpan, resultYVector.mutableSpan()));

        Vector<Digit, 16> resultVector(xorLength(resultX, resultY));
        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, false, absoluteXor(resultX, resultY, resultVector.mutableSpan())));
    }
    ASSERT(x.sign() != y.sign());

    // x ^ (-y) == x ^ ~(y-1) == ~(x ^ (y-1)) == -((x ^ (y-1)) + 1)
    auto computeResult = [&] (auto x, auto y) -> JSBigInt* {
        ASSERT(!x.sign());
        ASSERT(y.sign());

        auto xSpan = x.digits();
        auto ySpan = y.digits();
        Vector<Digit, 16> resultYVector(subOneLength(ySpan));
        auto resultY = normalize(absoluteSubOne(ySpan, resultYVector.mutableSpan()));

        Vector<Digit, 16> resultVector(xorLength(resultY, xSpan));
        auto result = normalize(absoluteXor(resultY, xSpan, resultVector.mutableSpan()));

        Vector<Digit, 16> finalResultVector(addOneLength(result));
        auto finalResult = absoluteAddOne(result, finalResultVector.mutableSpan());

        RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, true, finalResult));
    };

    // Assume that x is the positive BigInt.
    if (x.sign())
        return computeResult(y, x);
    return computeResult(x, y);
}

JSValue JSBigInt::bitwiseXor(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(bitwiseXorImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::bitwiseXor(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(bitwiseXorImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::bitwiseXor(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(bitwiseXorImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::leftShiftImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    if (x.isZero() || y.isZero())
        return { x };

    if (y.sign())
        return rightShiftByAbsolute(globalObject, x, y);

    return leftShiftByAbsolute(globalObject, x, y);
}

JSValue JSBigInt::leftShift(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(leftShiftImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::leftShift(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(leftShiftImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::leftShift(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(leftShiftImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
JSValue JSBigInt::leftShiftSlow(JSGlobalObject* globalObject, int32_t x, int32_t y)
{
    return tryConvertToBigInt32(leftShiftImpl(globalObject, Int32BigIntImpl { x }, Int32BigIntImpl { y }));
}
#endif

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::signedRightShiftImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    if (x.isZero() || y.isZero())
        return { x };

    if (y.sign())
        return leftShiftByAbsolute(globalObject, x, y);

    return rightShiftByAbsolute(globalObject, x, y);
}

JSValue JSBigInt::signedRightShift(JSGlobalObject* globalObject, JSBigInt* x, JSBigInt* y)
{
    return tryConvertToBigInt32(signedRightShiftImpl(globalObject, HeapBigIntImpl { x }, HeapBigIntImpl { y }));
}
#if USE(BIGINT32)
JSValue JSBigInt::signedRightShift(JSGlobalObject* globalObject, JSBigInt* x, int32_t y)
{
    return tryConvertToBigInt32(signedRightShiftImpl(globalObject, HeapBigIntImpl { x }, Int32BigIntImpl { y }));
}
JSValue JSBigInt::signedRightShift(JSGlobalObject* globalObject, int32_t x, JSBigInt* y)
{
    return tryConvertToBigInt32(signedRightShiftImpl(globalObject, Int32BigIntImpl { x }, HeapBigIntImpl { y }));
}
#endif

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::bitwiseNotImpl(JSGlobalObject* globalObject, BigIntImpl x)
{
    VM& vm = globalObject->vm();
    auto xSpan = x.digits();
    if (x.sign()) {
        // ~(-x) == ~(~(x-1)) == x-1
        Vector<Digit, 16> resultVector(subOneLength(xSpan));
        return tryCreateFromImpl(globalObject, vm, false, absoluteSubOne(xSpan, resultVector.mutableSpan()));
    } 
    // ~x == -x-1 == -(x+1)
    Vector<Digit, 16> resultVector(addOneLength(xSpan));
    auto result = absoluteAddOne(xSpan, resultVector.mutableSpan());
    return tryCreateFromImpl(globalObject, vm, true, result);
}

JSValue JSBigInt::bitwiseNot(JSGlobalObject* globalObject, JSBigInt* x)
{
    return tryConvertToBigInt32(bitwiseNotImpl(globalObject, HeapBigIntImpl { x }));
}

// {carry} must point to an initialized Digit and will either be incremented
// by one or left alone.
inline JSBigInt::Digit JSBigInt::digitAdd(Digit a, Digit b, Digit& carry)
{
    auto result = static_cast<TwoDigit>(a) + b;
    carry += static_cast<Digit>(result >> static_cast<int>(digitBits));
    return static_cast<Digit>(result);
}

// This compiles to slightly better machine code than repeated invocations
// of {digitAdd2}.
inline JSBigInt::Digit JSBigInt::digitAdd3(Digit a, Digit b, Digit c, Digit& carry)
{
    auto result = static_cast<TwoDigit>(a) + b + c;
    carry += static_cast<Digit>(result >> static_cast<int>(digitBits));
    return static_cast<Digit>(result);
}

// {borrow} must point to an initialized Digit and will either be incremented
// by one or left alone.
inline JSBigInt::Digit JSBigInt::digitSub(Digit a, Digit b, Digit& borrow)
{
    auto result = static_cast<TwoDigit>(a) - b;
    borrow += static_cast<Digit>(result >> static_cast<int>(digitBits)) & 1;
    return static_cast<Digit>(result);
}

// {borrow_out} will be set to 0 or 1.
inline JSBigInt::Digit JSBigInt::digitSub2(Digit a, Digit b, Digit borrowIn, Digit& borrowOut)
{
    auto subtrahend = static_cast<TwoDigit>(b) + borrowIn;
    auto result = static_cast<TwoDigit>(a) - subtrahend;
    borrowOut += static_cast<Digit>(result >> static_cast<int>(digitBits)) & 1;
    return static_cast<Digit>(result);
}

ALWAYS_INLINE std::tuple<JSBigInt::Digit, JSBigInt::Digit> JSBigInt::digitMul(Digit a, Digit b)
{
    TwoDigit result = static_cast<TwoDigit>(a) * static_cast<TwoDigit>(b);
    Digit high = static_cast<Digit>(result >> static_cast<int>(digitBits));
    Digit low = static_cast<Digit>(result);
    return { low, high };
}

// Raises {base} to the power of {exponent}. Does not check for overflow.
inline JSBigInt::Digit JSBigInt::digitPow(Digit base, Digit exponent)
{
    Digit result = 1ull;
    while (exponent > 0) {
        if (exponent & 1)
            result *= base;

        exponent >>= 1;
        base *= base;
    }

    return result;
}

// Returns the quotient.
// quotient = (high << digitBits + low - remainder) / divisor
inline JSBigInt::Digit JSBigInt::digitDiv(Digit high, Digit low, Digit divisor, Digit& remainder)
{
    ASSERT(high < divisor);
#if CPU(X86_64)
    Digit quotient;
    Digit rem;
    __asm__("divq  %[divisor]"
        // Outputs: {quotient} will be in rax, {rem} in rdx.
        : "=a"(quotient), "=d"(rem)
        // Inputs: put {high} into rdx, {low} into rax, and {divisor} into
        // any register or stack slot.
        : "d"(high), "a"(low), [divisor] "rm"(divisor));
    remainder = rem;
    return quotient;
#elif CPU(X86)
    Digit quotient;
    Digit rem;
    __asm__("divl  %[divisor]"
        // Outputs: {quotient} will be in eax, {rem} in edx.
        : "=a"(quotient), "=d"(rem)
        // Inputs: put {high} into edx, {low} into eax, and {divisor} into
        // any register or stack slot.
        : "d"(high), "a"(low), [divisor] "rm"(divisor));
    remainder = rem;
    return quotient;
#else
    // Fast path: if |high| is zero, the computation can be done within Digit range.
    // We do not need to have complicated path.
    if (!high) {
        ASSERT(divisor);
        remainder = low % divisor;
        return low / divisor;
    }

    static constexpr Digit halfDigitBase = 1ull << halfDigitBits;

    // Adapted from Warren, Hacker's Delight, p. 152.
    unsigned s = clz(divisor);
    // If {s} is digitBits here, it causes an undefined behavior.
    // But {s} is never digitBits since {divisor} is never zero here.
    ASSERT(s != digitBits);
    divisor <<= s;

    Digit vn1 = divisor >> halfDigitBits;
    Digit vn0 = divisor & halfDigitMask;

    // {sZeroMask} which is 0 if s == 0 and all 1-bits otherwise.
    // {s} can be 0. If {s} is 0, performing "low >> (digitBits - s)" must not be done since it causes an undefined behavior
    // since `>> digitBits` is undefied in C++. Quoted from C++ spec, "The type of the result is that of the promoted left operand.
    // The behavior is undefined if the right operand is negative, or greater than or equal to the length in bits of the promoted
    // left operand". We mask the right operand of the shift by {shiftMask} (`digitBits - 1`), which makes `digitBits - 0` zero.
    // This shifting produces a value which covers 0 < {s} <= (digitBits - 1) cases. {s} == digitBits never happen as we asserted.
    // Since {sZeroMask} clears the value in the case of {s} == 0, {s} == 0 case is also covered.
    static_assert(sizeof(CPURegister) == sizeof(Digit));
    Digit sZeroMask = static_cast<Digit>((-static_cast<CPURegister>(s)) >> (digitBits - 1));
    static constexpr unsigned shiftMask = digitBits - 1;
    Digit un32 = (high << s) | ((low >> ((digitBits - s) & shiftMask)) & sZeroMask);

    Digit un10 = low << s;
    Digit un1 = un10 >> halfDigitBits;
    Digit un0 = un10 & halfDigitMask;
    Digit q1 = un32 / vn1;
    Digit rhat = un32 - q1 * vn1;

    while (q1 >= halfDigitBase || q1 * vn0 > rhat * halfDigitBase + un1) {
        q1--;
        rhat += vn1;
        if (rhat >= halfDigitBase)
            break;
    }

    Digit un21 = un32 * halfDigitBase + un1 - q1 * divisor;
    Digit q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;

    while (q0 >= halfDigitBase || q0 * vn0 > rhat * halfDigitBase + un0) {
        q0--;
        rhat += vn1;
        if (rhat >= halfDigitBase)
            break;
    }

    remainder = (un21 * halfDigitBase + un0 - q0 * divisor) >> s;
    return q1 * halfDigitBase + q0;
#endif
}

// Multiplies {source} with {factor} and adds {summand} to the result.
// {result} and {source} may be the same BigInt for inplace modification.
// Multiplies {this} with {factor} and adds {summand} to the result.
void JSBigInt::multiplyAdd(std::span<const Digit> source, Digit factor, Digit summand, std::span<Digit> result)
{
    RELEASE_ASSERT(result.size() >= source.size());

    Digit carry = summand;
    Digit high = 0;
    size_t i = 0;
    for (; i < source.size(); i++) {
        // Compute this round's multiplication.
        auto [current, newHigh] = digitMul(source[i], factor);

        // Add last round's carryovers.
        Digit newCarry = 0;
        current = digitAdd(current, high, newCarry);
        current = digitAdd(current, carry, newCarry);

        // Store result and prepare for next round.
        result[i] = current;
        carry = newCarry;
        high = newHigh;
    }

    if (result.size() > i) {
        result[i++] = carry + high;

        // Current callers don't pass in such large results, but let's be robust.
        while (i < result.size())
            result[i++] = 0;
    } else
        ASSERT(!(carry + high));
}

bool JSBigInt::equals(JSBigInt* x, JSBigInt* y)
{
    if (x->sign() != y->sign())
        return false;

    if (x->length() != y->length())
        return false;

    for (unsigned i = 0; i < x->length(); i++) {
        if (x->digit(i) != y->digit(i))
            return false;
    }

    return true;
}

template <typename BigIntImpl1, typename BigIntImpl2>
inline JSBigInt::ComparisonResult JSBigInt::absoluteCompare(BigIntImpl1 x, BigIntImpl2 y)
{
    ASSERT(!x.length() || x.digit(x.length() - 1));
    ASSERT(!y.length() || y.digit(y.length() - 1));

    int diff = x.length() - y.length();
    if (diff)
        return diff < 0 ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    int i = x.length() - 1;
    while (i >= 0 && x.digit(i) == y.digit(i))
        i--;

    if (i < 0)
        return ComparisonResult::Equal;

    return x.digit(i) > y.digit(i) ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ComparisonResult JSBigInt::compareImpl(BigIntImpl1 x, BigIntImpl2 y)
{
    bool xSign = x.sign();

    if (xSign != y.sign())
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    ComparisonResult result = absoluteCompare(x, y);
    if (result == ComparisonResult::GreaterThan)
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    if (result == ComparisonResult::LessThan)
        return xSign ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;

    return ComparisonResult::Equal; 
}

JSBigInt::ComparisonResult JSBigInt::compare(JSBigInt* x, JSBigInt* y)
{
    return compareImpl(HeapBigIntImpl { x }, HeapBigIntImpl { y });
}

JSBigInt::ComparisonResult JSBigInt::compare(int32_t x, JSBigInt* y)
{
    return compareImpl(Int32BigIntImpl { x }, HeapBigIntImpl { y });
}

JSBigInt::ComparisonResult JSBigInt::compare(JSBigInt* x, int32_t y)
{
    return compareImpl(HeapBigIntImpl { x }, Int32BigIntImpl { y });
}

JSBigInt::ComparisonResult JSBigInt::compare(JSBigInt* x, int64_t y)
{
    return compareImpl(HeapBigIntImpl { x }, Int64BigIntImpl { y });
}

JSBigInt::ComparisonResult JSBigInt::compare(JSValue x, int64_t y)
{
    ASSERT(x.isBigInt());
#if USE(BIGINT32)
    if (x.isBigInt32())
        return compareImpl(Int32BigIntImpl { x.bigInt32AsInt32() }, Int64BigIntImpl { y });
#endif
    return compare(x.asHeapBigInt(), y);
}

JSBigInt::ComparisonResult JSBigInt::compare(JSBigInt* x, uint64_t y)
{
    return compareImpl(HeapBigIntImpl { x }, Int64BigIntImpl { y });
}

JSBigInt::ComparisonResult JSBigInt::compare(JSValue x, uint64_t y)
{
    ASSERT(x.isBigInt());
#if USE(BIGINT32)
    if (x.isBigInt32())
        return compareImpl(Int32BigIntImpl { x.bigInt32AsInt32() }, Int64BigIntImpl { y });
#endif
    return compare(x.asHeapBigInt(), y);
}

JSBigInt::ComparisonResult JSBigInt::compare(JSValue x, JSValue y)
{
    ASSERT(x.isBigInt() && y.isBigInt());
#if USE(BIGINT32)
    if (x.isBigInt32() && y.isBigInt32()) {
        int32_t x1 = x.asBigInt32();
        int32_t y1 = y.asBigInt32();
        if (x1 == y1)
            return JSBigInt::ComparisonResult::Equal;
        if (x1 < y1)
            return JSBigInt::ComparisonResult::LessThan;
        return JSBigInt::ComparisonResult::GreaterThan;
    }
    if (x.isBigInt32())
        return compare(x.bigInt32AsInt32(), y.asHeapBigInt());
    if (y.isBigInt32())
        return compare(x.asHeapBigInt(), y.bigInt32AsInt32());
#endif
    return compare(x.asHeapBigInt(), y.asHeapBigInt());
}

std::span<JSBigInt::Digit> JSBigInt::addTextbook(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
    RELEASE_ASSERT(x.size() >= y.size());
    RELEASE_ASSERT(result.size() >= (x.size() + 1));
    Digit carry = 0;
    size_t i = 0;
    for (; i < y.size(); i++) {
        Digit newCarry = 0;
        result[i] = digitAdd3(x[i], y[i], carry, newCarry);
        carry = newCarry;
    }

    for (; i < x.size(); i++) {
        Digit newCarry = 0;
        result[i] = digitAdd(x[i], carry, newCarry);
        carry = newCarry;
    }

    result[i++] = carry;
    return result.first(i);
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::absoluteAdd(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y, bool resultSign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.length() < y.length())
        RELEASE_AND_RETURN(scope, absoluteAdd(globalObject, y, x, resultSign));

    if (x.isZero()) {
        ASSERT(y.isZero());
        return { x };
    }

    if (y.isZero()) {
        if (resultSign == x.sign())
            return { x };
        RELEASE_AND_RETURN(scope, unaryMinusImpl(globalObject, x));
    }

    Vector<Digit, 16> result(x.length() + 1);
    auto span = addTextbook(x.digits(), y.digits(), result.mutableSpan());
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, resultSign, span));
}

std::span<JSBigInt::Digit> JSBigInt::subTextbook(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
    RELEASE_ASSERT(x.size() >= y.size());
    RELEASE_ASSERT(result.size() >= x.size());
    Digit borrow = 0;
    size_t i = 0;
    for (; i < y.size(); i++) {
        Digit newBorrow = 0;
        result[i] = digitSub2(x[i], y[i], borrow, newBorrow);
        borrow = newBorrow;
    }

    for (; i < x.size(); i++) {
        Digit newBorrow = 0;
        result[i] = digitSub(x[i], borrow, newBorrow);
        borrow = newBorrow;
    }

    ASSERT(!borrow);
    return result.first(x.size());
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::absoluteSub(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y, bool resultSign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ComparisonResult comparisonResult = absoluteCompare(x, y);
    ASSERT(x.length() >= y.length());
    ASSERT(comparisonResult == ComparisonResult::GreaterThan || comparisonResult == ComparisonResult::Equal);

    if (x.isZero()) {
        ASSERT(y.isZero());
        return { x };
    }

    if (y.isZero()) {
        if (resultSign == x.sign())
            return ImplResult { x };
        RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, x));
    }

    if (comparisonResult == ComparisonResult::Equal)
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    Vector<Digit, 16> result(x.length());
    auto span = subTextbook(x.digits(), y.digits(), result.mutableSpan());
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, resultSign, span));
}

// Returns whether (factor1 * factor2) > (high << digitBits) + low.
inline bool JSBigInt::productGreaterThan(Digit factor1, Digit factor2, Digit high, Digit low)
{
    auto [resultLow, resultHigh] = digitMul(factor1, factor2);
    return resultHigh > high || (resultHigh == high && resultLow > low);
}

// Helper for Absolute{And,AndNot,Or,Xor}.
// Performs the given binary {op} on digit pairs of {x} and {y}; when the
// end of the shorter of the two is reached, {extraDigits} configures how
// remaining digits in the longer input are handled: copied to the result
// or ignored.
// Example:
//       y:             [ y2 ][ y1 ][ y0 ]
//       x:       [ x3 ][ x2 ][ x1 ][ x0 ]
//                   |     |     |     |
//                (Copy)  (op)  (op)  (op)
//                   |     |     |     |
//                   v     v     v     v
// result: [  0 ][ x3 ][ r2 ][ r1 ][ r0 ]
template<typename BitwiseOp>
inline std::span<JSBigInt::Digit> JSBigInt::absoluteBitwiseOp(std::span<const Digit> x, std::span<const Digit> y, ExtraDigitsHandling extraDigits, BitwiseOp&& op, std::span<Digit> result)
{
    if (x.size() < y.size())
        std::swap(x, y);

    ASSERT(x.size() >= y.size());

    size_t numPairs = y.size();
    size_t maxLength = x.size();

    size_t resultLength = extraDigits == ExtraDigitsHandling::Copy ? maxLength : numPairs;
    RELEASE_ASSERT(result.size() >= resultLength);

    size_t i = 0;
    for (; i < numPairs; i++)
        result[i] = op(x[i], y[i]);

    if (extraDigits == ExtraDigitsHandling::Copy) {
        if (numPairs != maxLength)
            memcpySpan(result.subspan(numPairs), x.subspan(numPairs));
    }

    return result.first(resultLength);
}

std::span<JSBigInt::Digit> JSBigInt::absoluteAnd(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
    ASSERT(result.size() >= andLength(x, y));
    auto digitOperation = [](Digit a, Digit b) {
        return a & b;
    };
    return absoluteBitwiseOp(x, y, ExtraDigitsHandling::Skip, digitOperation, result);
}

std::span<JSBigInt::Digit> JSBigInt::absoluteOr(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
    ASSERT(result.size() >= orLength(x, y));
    auto digitOperation = [](Digit a, Digit b) {
        return a | b;
    };
    return absoluteBitwiseOp(x, y, ExtraDigitsHandling::Copy, digitOperation, result);
}

std::span<JSBigInt::Digit> JSBigInt::absoluteAndNot(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
    // x & ~y
    RELEASE_ASSERT(result.size() >= andNotLength(x, y));

    size_t i = 0;
    for (; i < std::min(x.size(), y.size()); i++)
        result[i] = x[i] & ~y[i];

    for (; i < x.size(); ++i)
        result[i] = x[i];

    return result.first(x.size());
}

std::span<JSBigInt::Digit> JSBigInt::absoluteXor(std::span<const Digit> x, std::span<const Digit> y, std::span<Digit> result)
{
    ASSERT(result.size() >= xorLength(x, y));
    auto digitOperation = [](Digit a, Digit b) {
        return a ^ b;
    };
    return absoluteBitwiseOp(x, y, ExtraDigitsHandling::Copy, digitOperation, result);
}

std::span<JSBigInt::Digit> JSBigInt::absoluteAddOne(std::span<const Digit> x, std::span<Digit> result)
{
    ASSERT(result.size() >= addOneLength(x));
    Digit carry = 1;
    size_t i = 0;
    for (; i < x.size(); i++) {
        Digit newCarry = 0;
        result[i] = digitAdd(x[i], carry, newCarry);
        carry = newCarry;
    }
    if (carry)
        result[i++] = carry;
    return result.first(i);
}

std::span<JSBigInt::Digit> JSBigInt::absoluteSubOne(std::span<const Digit> x, std::span<Digit> result)
{
    ASSERT(!x.empty());
    ASSERT(result.size() >= subOneLength(x));
    Digit borrow = 1;
    for (size_t i = 0; i < x.size(); i++) {
        Digit newBorrow = 0;
        result[i] = digitSub(x[i], borrow, newBorrow);
        borrow = newBorrow;
    }
    ASSERT(!borrow);
    return result.first(x.size());
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::leftShiftByAbsolute(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto optionalShift = toShiftAmount(y);
    if (!optionalShift) {
        throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
        return nullptr;
    }

    Digit shift = *optionalShift;
    size_t digitShift = static_cast<size_t>(shift / digitBits);
    size_t bitsShift = static_cast<size_t>(shift % digitBits);
    auto xSpan = x.digits();
    size_t length = xSpan.size();
    bool grow = bitsShift && (xSpan[length - 1] >> (digitBits - bitsShift));
    size_t resultLength = length + digitShift + grow;
    if (resultLength > maxLength) {
        throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
        return nullptr;
    }

    Vector<Digit, 16> resultVector(resultLength);
    auto result = resultVector.mutableSpan();
    if (!bitsShift) {
        size_t i = 0;
        for (; i < digitShift; i++)
            result[i] = 0ul;

        for (; i < resultLength; i++)
            result[i] = xSpan[i - digitShift];
    } else {
        Digit carry = 0;
        for (size_t i = 0; i < digitShift; i++)
            result[i] = 0ul;

        for (size_t i = 0; i < length; i++) {
            Digit d = xSpan[i];
            result[i + digitShift] = (d << bitsShift) | carry;
            carry = d >> (digitBits - bitsShift);
        }

        if (grow)
            result[length + digitShift] = carry;
        else
            ASSERT(!carry);
    }

    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, x.sign(), result));
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::rightShiftByAbsolute(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto xSpan = x.digits();
    size_t length = xSpan.size();
    bool sign = x.sign();
    auto optionalShift = toShiftAmount(y);
    if (!optionalShift)
        RELEASE_AND_RETURN(scope, rightShiftByMaximum(globalObject, sign));

    Digit shift = *optionalShift;
    size_t digitalShift = static_cast<size_t>(shift / digitBits);
    size_t bitsShift = static_cast<size_t>(shift % digitBits);
    if (length <= digitalShift)
        RELEASE_AND_RETURN(scope, rightShiftByMaximum(globalObject, sign));

    size_t resultLength = length - digitalShift;

    // For negative numbers, round down if any bit was shifted out (so that e.g.
    // -5n >> 1n == -3n and not -2n). Check now whether this will happen and
    // whether it can cause overflow into a new digit. If we allocate the result
    // large enough up front, it avoids having to do a second allocation later.
    bool mustRoundDown = false;
    if (sign) {
        const Digit mask = (static_cast<Digit>(1) << bitsShift) - 1;
        if (xSpan[digitalShift] & mask)
            mustRoundDown = true;
        else {
            for (size_t i = 0; i < digitalShift; i++) {
                if (xSpan[i]) {
                    mustRoundDown = true;
                    break;
                }
            }
        }
    }

    // If bitsShift is non-zero, it frees up bits, preventing overflow.
    if (mustRoundDown && !bitsShift) {
        // Overflow cannot happen if the most significant digit has unset bits.
        Digit msd = xSpan[length - 1];
        bool roundingCanOverflow = !static_cast<Digit>(~msd);
        if (roundingCanOverflow)
            resultLength++;
    }

    ASSERT(resultLength <= length);
    Vector<Digit, 16> resultVector(resultLength);
    auto result = resultVector.mutableSpan();

    if (!bitsShift) {
        result[resultLength - 1] = 0;
        for (size_t i = digitalShift; i < length; i++)
            result[i - digitalShift] = xSpan[i];
    } else {
        Digit carry = xSpan[digitalShift] >> bitsShift;
        size_t last = length - digitalShift - 1;
        for (size_t i = 0; i < last; i++) {
            Digit d = xSpan[i + digitalShift + 1];
            result[i] = (d << (digitBits - bitsShift)) | carry;
            carry = d >> bitsShift;
        }
        result[last] = carry;
    }

    if (sign) {
        if (mustRoundDown) {
            // Since the result is negative, rounding down means adding one to
            // its absolute value. This cannot overflow.
            result = normalize(result);
            Vector<Digit, 16> finalResultVector(addOneLength(result));
            auto finalResult = absoluteAddOne(result, finalResultVector.mutableSpan());
            RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, sign, finalResult));
        }
    }

    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, sign, result));
}

JSBigInt::ImplResult JSBigInt::rightShiftByMaximum(JSGlobalObject* globalObject, bool sign)
{
    if (sign)
        return createFrom(globalObject, -1);

    return createZero(globalObject);
}

// Lookup table for the maximum number of bits required per character of a
// base-N string representation of a number. To increase accuracy, the array
// value is the actual value multiplied by 32. To generate this table:
// for (var i = 0; i <= 36; i++) { print(Math.ceil(Math.log2(i) * 32) + ","); }
constexpr uint8_t maxBitsPerCharTable[] = {
    0,   0,   32,  51,  64,  75,  83,  90,  96, // 0..8
    102, 107, 111, 115, 119, 122, 126, 128,     // 9..16
    131, 134, 136, 139, 141, 143, 145, 147,     // 17..24
    149, 151, 153, 154, 156, 158, 159, 160,     // 25..32
    162, 163, 165, 166,                         // 33..36
};

static constexpr unsigned bitsPerCharTableShift = 5;
static constexpr size_t bitsPerCharTableMultiplier = 1u << bitsPerCharTableShift;

// Compute (an overapproximation of) the length of the resulting string:
// Divide bit length of the BigInt by bits representable per character.
uint64_t JSBigInt::calculateMaximumCharactersRequired(unsigned length, unsigned radix, Digit lastDigit, bool sign)
{
    unsigned leadingZeros = clz(lastDigit);

    size_t bitLength = length * digitBits - leadingZeros;

    // Maximum number of bits we can represent with one character. We'll use this
    // to find an appropriate chunk size below.
    uint8_t maxBitsPerChar = maxBitsPerCharTable[radix];

    // For estimating result length, we have to be pessimistic and work with
    // the minimum number of bits one character can represent.
    uint8_t minBitsPerChar = maxBitsPerChar - 1;

    // Perform the following computation with uint64_t to avoid overflows.
    uint64_t maximumCharactersRequired = bitLength;
    maximumCharactersRequired *= bitsPerCharTableMultiplier;

    // Round up.
    maximumCharactersRequired += minBitsPerChar - 1;
    maximumCharactersRequired /= minBitsPerChar;
    maximumCharactersRequired += sign;
    
    return maximumCharactersRequired;
}

String JSBigInt::toStringBasePowerOfTwo(VM& vm, JSGlobalObject* nullOrGlobalObjectForOOM, JSBigInt* x, unsigned radix)
{
    ASSERT(hasOneBitSet(radix));
    ASSERT(radix >= 2 && radix <= 32);
    ASSERT(!x->isZero());

    const unsigned length = x->length();
    const bool sign = x->sign();
    const unsigned bitsPerChar = ctz(radix);
    const unsigned charMask = radix - 1;
    // Compute the length of the resulting string: divide the bit length of the
    // BigInt by the number of bits representable per character (rounding up).
    const Digit msd = x->digit(length - 1);

    const unsigned msdLeadingZeros = clz(msd);

    const size_t bitLength = length * digitBits - msdLeadingZeros;
    const size_t charsRequired = (bitLength + bitsPerChar - 1) / bitsPerChar + sign;

    if (charsRequired > JSString::MaxLength) {
        if (nullOrGlobalObjectForOOM) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
        }
        return String();
    }

    Vector<Latin1Character> resultString(charsRequired);
    Digit digit = 0;
    // Keeps track of how many unprocessed bits there are in {digit}.
    unsigned availableBits = 0;
    int pos = static_cast<int>(charsRequired - 1);
    for (unsigned i = 0; i < length - 1; i++) {
        Digit newDigit = x->digit(i);
        // Take any leftover bits from the last iteration into account.
        int current = (digit | (newDigit << availableBits)) & charMask;
        resultString[pos--] = radixDigits[current];
        int consumedBits = bitsPerChar - availableBits;
        digit = newDigit >> consumedBits;
        availableBits = digitBits - consumedBits;
        while (availableBits >= bitsPerChar) {
            resultString[pos--] = radixDigits[digit & charMask];
            digit >>= bitsPerChar;
            availableBits -= bitsPerChar;
        }
    }
    // Take any leftover bits from the last iteration into account.
    int current = (digit | (msd << availableBits)) & charMask;
    resultString[pos--] = radixDigits[current];
    digit = msd >> (bitsPerChar - availableBits);
    while (digit) {
        resultString[pos--] = radixDigits[digit & charMask];
        digit >>= bitsPerChar;
    }

    if (sign)
        resultString[pos--] = '-';

    ASSERT(pos == -1);
    return StringImpl::adopt(WTF::move(resultString));
}

String JSBigInt::toStringGeneric(VM& vm, JSGlobalObject* nullOrGlobalObjectForOOM, JSBigInt* x, unsigned radix)
{
    // FIXME: [JSC] Revisit usage of Vector into JSBigInt::toString
    // https://bugs.webkit.org/show_bug.cgi?id=180671
    Vector<Latin1Character> resultString;

    ASSERT(radix >= 2 && radix <= 36);
    ASSERT(!x->isZero());

    unsigned length = x->length();
    bool sign = x->sign();

    uint8_t maxBitsPerChar = maxBitsPerCharTable[radix];
    uint64_t maximumCharactersRequired = calculateMaximumCharactersRequired(length, radix, x->digit(length - 1), sign);

    if (maximumCharactersRequired > JSString::MaxLength) {
        if (nullOrGlobalObjectForOOM) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
        }
        return String();
    }

    Digit lastDigit;
    if (length == 1)
        lastDigit = x->digit(0);
    else {
        unsigned chunkChars = digitBits * bitsPerCharTableMultiplier / maxBitsPerChar;
        Digit chunkDivisor = digitPow(radix, chunkChars);

        // By construction of chunkChars, there can't have been overflow.
        ASSERT(chunkDivisor);

        // {rest} holds the part of the BigInt that we haven't looked at yet.
        // Not to be confused with "remainder"!
        // In the first round, divide the input, allocating a new BigInt for
        // the result == rest; from then on divide the rest in-place.
        Vector<Digit, 16> rest(length);
        std::span<const Digit> dividend = x->digits();
        do {
            Digit chunk;
            std::span<Digit> quotient = divideSingle(rest.mutableSpan(), chunk, dividend, chunkDivisor);

            for (unsigned i = 0; i < chunkChars; i++) {
                resultString.append(radixDigits[chunk % radix]);
                chunk /= radix;
            }
            ASSERT(!chunk);

            // Update dividend to point to the quotient for next iteration.
            // The quotient.size() tells us how many digits are non-zero.
            dividend = normalize(quotient);
        } while (dividend.size() > 1);

        lastDigit = dividend.empty() ? 0 : dividend[0];
    }

    do {
        resultString.append(radixDigits[lastDigit % radix]);
        lastDigit /= radix;
    } while (lastDigit > 0);
    ASSERT(resultString.size());
    ASSERT(resultString.size() <= static_cast<size_t>(maximumCharactersRequired));

    // Remove leading zeroes.
    unsigned newSizeNoLeadingZeroes = resultString.size();
    while (newSizeNoLeadingZeroes  > 1 && resultString[newSizeNoLeadingZeroes - 1] == '0')
        newSizeNoLeadingZeroes--;

    resultString.shrink(newSizeNoLeadingZeroes);

    if (sign)
        resultString.append('-');

    std::ranges::reverse(resultString);

    return StringImpl::adopt(WTF::move(resultString));
}

double JSBigInt::toNumber(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(globalObject, scope, "Conversion from 'BigInt' to 'number' is not allowed."_s);
    return 0.0;
}

template <typename CharType>
JSValue JSBigInt::parseInt(JSGlobalObject* globalObject, std::span<const CharType> data, ErrorParseMode errorParseMode)
{
    VM& vm = globalObject->vm();

    size_t p = 0;
    while (p < data.size() && isStrWhiteSpace(data[p]))
        ++p;

    // Check Radix from first characters
    if (p + 1 < data.size() && data[p] == '0') {
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'b'))
            return parseInt(globalObject, vm, data, p + 2, 2, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'x'))
            return parseInt(globalObject, vm, data, p + 2, 16, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'o'))
            return parseInt(globalObject, vm, data, p + 2, 8, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
    }

    ParseIntSign sign = ParseIntSign::Unsigned;
    if (p < data.size()) {
        if (data[p] == '-') {
            sign = ParseIntSign::Signed;
            ++p;
        } else if (data[p] == '+')
            ++p;
    }

    return parseInt(globalObject, vm, data, p, 10, errorParseMode, sign);
}

template <typename CharType>
JSValue JSBigInt::parseInt(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, std::span<const CharType> data, unsigned startIndex, unsigned radix, ErrorParseMode errorParseMode, ParseIntSign sign, ParseIntMode parseMode)
{
    size_t p = startIndex;

    if (parseMode != ParseIntMode::AllowEmptyString && startIndex == data.size()) {
        ASSERT(nullOrGlobalObjectForOOM);
        if (errorParseMode == ErrorParseMode::ThrowExceptions) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwVMError(nullOrGlobalObjectForOOM, scope, createSyntaxError(nullOrGlobalObjectForOOM, "Failed to parse String to BigInt"_s));
        }
        return JSValue();
    }

    // Skipping leading zeros
    while (p < data.size() && data[p] == '0')
        ++p;

    int endIndex = data.size() - 1;
    // Removing trailing spaces
    while (endIndex >= static_cast<int>(p) && isStrWhiteSpace(data[endIndex]))
        --endIndex;

    size_t length = endIndex + 1;

    if (p == length) {
#if USE(BIGINT32)
        return jsBigInt32(0);
#else
        return createZero(nullOrGlobalObjectForOOM, vm);
#endif
    }

    unsigned lengthLimitForBigInt32;
#if USE(BIGINT32)
    static_assert(sizeof(Digit) >= sizeof(uint64_t));
    // The idea is to pick the limit such that:
    // radix ** lengthLimitForBigInt32 >= INT32_MAX
    // radix ** (lengthLimitForBigInt32 - 1) <= INT32_MAX
#if ASSERT_ENABLED
    auto limitWorks = [&] {
        double lengthLimit = lengthLimitForBigInt32;
        double lowerLimit = pow(static_cast<double>(radix), lengthLimit - 1);
        double upperLimit = pow(static_cast<double>(radix), lengthLimit);
        double target = std::numeric_limits<int32_t>::max();
        return lowerLimit <= target && target <= upperLimit && upperLimit <= std::numeric_limits<int64_t>::max();
    };
#endif
    switch (radix) {
    case 2:
        lengthLimitForBigInt32 = 31;
        ASSERT(limitWorks());
        break;
    case 8:
        lengthLimitForBigInt32 = 11;
        ASSERT(limitWorks());
        break;
    case 10:
        lengthLimitForBigInt32 = 10;
        ASSERT(limitWorks());
        break;
    case 16:
        lengthLimitForBigInt32 = 8;
        ASSERT(limitWorks());
        break;
    default:
        lengthLimitForBigInt32 = 1;
        break;
    }
#else
    // The idea is to pick the largest limit such that:
    // radix ** lengthLimitForBigInt32 <= INT32_MAX
#if ASSERT_ENABLED
    auto limitWorks = [&] {
        double lengthLimit = lengthLimitForBigInt32;
        double valueLimit = pow(static_cast<double>(radix), lengthLimit);
        double overValueLimit = pow(static_cast<double>(radix), lengthLimit + 1);
        double target = std::numeric_limits<int32_t>::max();
        return valueLimit <= target && target < overValueLimit;
    };
#endif
    switch (radix) {
    case 2:
        lengthLimitForBigInt32 = 30;
        ASSERT(limitWorks());
        break;
    case 8:
        lengthLimitForBigInt32 = 10;
        ASSERT(limitWorks());
        break;
    case 10:
        lengthLimitForBigInt32 = 9;
        ASSERT(limitWorks());
        break;
    case 16:
        lengthLimitForBigInt32 = 7;
        ASSERT(limitWorks());
        break;
    default:
        lengthLimitForBigInt32 = 1;
        break;
    }
#endif // USE(BIGINT32)

    unsigned limit0 = '0' + (radix < 10 ? radix : 10);
    unsigned limita = 'a' + (static_cast<int32_t>(radix) - 10);
    unsigned limitA = 'A' + (static_cast<int32_t>(radix) - 10);
    unsigned initialLength = length - p;
    Vector<Digit, 16> resultVector;
    while (p < length) {
        Checked<uint64_t, CrashOnOverflow> digit = 0;
        Checked<uint64_t, CrashOnOverflow> multiplier = 1;
        for (unsigned i = 0; i < lengthLimitForBigInt32 && p < length; ++i, ++p) {
            digit *= radix;
            multiplier *= radix;
            if (data[p] >= '0' && data[p] < limit0)
                digit += static_cast<uint64_t>(data[p] - '0');
            else if (data[p] >= 'a' && data[p] < limita)
                digit += static_cast<uint64_t>(data[p] - 'a' + 10);
            else if (data[p] >= 'A' && data[p] < limitA)
                digit += static_cast<uint64_t>(data[p] - 'A' + 10);
            else {
                if (errorParseMode == ErrorParseMode::ThrowExceptions) {
                    auto scope = DECLARE_THROW_SCOPE(vm);
                    ASSERT(nullOrGlobalObjectForOOM);
                    throwVMError(nullOrGlobalObjectForOOM, scope, createSyntaxError(nullOrGlobalObjectForOOM, "Failed to parse String to BigInt"_s));
                }
                return JSValue();
            }
        }

        if (resultVector.isEmpty()) {
            if (p == length) {
                ASSERT(digit <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
                int64_t maybeResult = digit;
                ASSERT(maybeResult >= 0);
                if (sign == ParseIntSign::Signed)
                    maybeResult *= -1;

                if (static_cast<int64_t>(static_cast<int32_t>(maybeResult)) == maybeResult) {
#if USE(BIGINT32)
                    return jsBigInt32(static_cast<int32_t>(maybeResult));
#else
                    return JSBigInt::createFrom(nullOrGlobalObjectForOOM, vm, static_cast<int32_t>(maybeResult));
#endif
                }
            }

            auto computeLength = [](unsigned radix, unsigned charcount) -> std::optional<unsigned> {
                ASSERT(2 <= radix && radix <= 36);

                size_t bitsPerChar = maxBitsPerCharTable[radix];
                size_t chars = charcount;
                const unsigned roundup = bitsPerCharTableMultiplier - 1;
                if (chars <= (std::numeric_limits<size_t>::max() - roundup) / bitsPerChar) {
                    size_t bitsMin = bitsPerChar * chars;

                    // Divide by 32 (see table), rounding up.
                    bitsMin = (bitsMin + roundup) >> bitsPerCharTableShift;
                    if (bitsMin <= static_cast<size_t>(maxInt)) {
                        // Divide by kDigitsBits, rounding up.
                        unsigned length = (bitsMin + digitBits - 1) / digitBits;
                        if (length <= maxLength)
                            return length;
                    }
                }

                return std::nullopt;
            };

            auto length = computeLength(radix, initialLength);
            if (!length) [[unlikely]] {
                if (nullOrGlobalObjectForOOM) {
                    auto scope = DECLARE_THROW_SCOPE(vm);
                    throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope, "BigInt generated from this operation is too big"_s);
                }
                return JSValue();
            }

            resultVector.fill(0, length.value());
        }

        ASSERT(static_cast<uint64_t>(static_cast<Digit>(multiplier)) == multiplier);
        ASSERT(static_cast<uint64_t>(static_cast<Digit>(digit)) == digit);
        multiplyAdd(resultVector.span(), static_cast<Digit>(multiplier), static_cast<Digit>(digit), resultVector.mutableSpan());
    }

    return tryCreateFromImpl(nullOrGlobalObjectForOOM, vm, sign == ParseIntSign::Signed, resultVector.span());
}

JSObject* JSBigInt::toObject(JSGlobalObject* globalObject) const
{
    return BigIntObject::create(globalObject->vm(), globalObject, const_cast<JSBigInt*>(this));
}

bool JSBigInt::equalsToNumber(JSValue numValue)
{
    ASSERT(numValue.isNumber());
    
    if (numValue.isInt32())
        return equalsToInt32(numValue.asInt32());
    
    double value = numValue.asDouble();
    return compareToDouble(this, value) == ComparisonResult::Equal;
}

bool JSBigInt::equalsToInt32(int32_t value)
{
    if (!value)
        return this->isZero();
    return (this->length() == 1) && (this->sign() == (value < 0)) && (this->digit(0) == static_cast<Digit>(std::abs(static_cast<int64_t>(value))));
}

JSBigInt::ComparisonResult JSBigInt::compareToDouble(JSBigInt* x, double y)
{
    return compareToDouble(HeapBigIntImpl { x }, y);
}

JSBigInt::ComparisonResult JSBigInt::compareToDouble(double x, JSBigInt* y)
{
    return compareToDouble(x, HeapBigIntImpl { y });
}

template <typename BigIntImpl>
JSBigInt::ComparisonResult JSBigInt::compareToDouble(BigIntImpl x, double y)
{
    // This algorithm expect that the double format is IEEE 754

    uint64_t doubleBits = std::bit_cast<uint64_t>(y);
    int rawExponent = static_cast<int>(doubleBits >> 52) & 0x7FF;

    // Handle finite doubles for {y}.
    if (rawExponent == 0x7FF) {
        if (std::isnan(y))
            return ComparisonResult::Undefined;

        return (y == std::numeric_limits<double>::infinity()) ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    }

    bool xSign = x.sign();
    
    // Note that this is different from the double's sign bit for -0. That's
    // intentional because -0 must be treated like 0.
    bool ySign = y < 0;
    if (xSign != ySign)
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    if (!y) {
        // If {y} is zero, then ySign is false and xSign must be false.
        ASSERT(!xSign);
        return x.isZero() ? ComparisonResult::Equal : ComparisonResult::GreaterThan;
    }

    if (x.isZero()) {
        // If {x} is zero, then xSign is false and ySign must be false which indicates that {y} is greater than zero.
        ASSERT(!ySign && y > 0);
        return ComparisonResult::LessThan;
    }

    // Right now, only two cases left:
    //     {x} >= 1 and {y} > 0
    //     {x} <= -1 and {y} < 0

    // Non-finite doubles are handled above.
    ASSERT(rawExponent != 0x7FF);
    int exponent = rawExponent - 0x3FF;
    if (exponent < 0) {
        // The absolute value of the double is less than 1. Only 0n has an
        // absolute value smaller than that, but we've already covered that case.
        // Note that this also handles denormal doubles for {y}.
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    }

    int xLength = x.length();
    Digit xMSD = x.digit(xLength - 1);
    int msdLeadingZeros = clz(xMSD);

    int xBitLength = xLength * digitBits - msdLeadingZeros;
    int yBitLength = exponent + 1;
    if (xBitLength < yBitLength)
        return xSign? ComparisonResult::GreaterThan : ComparisonResult::LessThan;

    if (xBitLength > yBitLength)
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    
    // At this point, we know that signs and bit lengths (i.e. position of
    // the most significant bit in exponent-free representation) are identical.
    // {x} is not zero, {y} is finite and not denormal.
    // Now we virtually convert the double to an integer by shifting its
    // mantissa according to its exponent, so it will align with the BigInt {x},
    // and then we compare them bit for bit until we find a difference or the
    // least significant bit.
    //                    <----- 52 ------> <-- virtual trailing zeroes -->
    // y / mantissa:     1yyyyyyyyyyyyyyyyy 0000000000000000000000000000000
    // x / digits:    0001xxxx xxxxxxxx xxxxxxxx ...
    //                    <-->          <------>
    //              msdTopBit         digitBits
    //
    uint64_t mantissa = doubleBits & 0x000FFFFFFFFFFFFF;
    mantissa |= 0x0010000000000000;
    const int mantissaTopBit = 52; // 0-indexed.

    // 0-indexed position of {x}'s most significant bit within the {msd}.
    int msdTopBit = digitBits - 1 - msdLeadingZeros;
    ASSERT(msdTopBit == static_cast<int>((xBitLength - 1) % digitBits));
    
    // Shifted chunk of {mantissa} for comparing with {digit}.
    Digit compareMantissa;

    // Number of unprocessed bits in {mantissa}. We'll keep them shifted to
    // the left (i.e. most significant part) of the underlying uint64_t.
    int remainingMantissaBits = 0;
    
    // First, compare the most significant digit against the beginning of
    // the mantissa and then we align them.
    if (msdTopBit < mantissaTopBit) {
        remainingMantissaBits = (mantissaTopBit - msdTopBit);
        compareMantissa = static_cast<Digit>(mantissa >> remainingMantissaBits);
        mantissa = mantissa << (64 - remainingMantissaBits);
    } else {
        compareMantissa = static_cast<Digit>(mantissa << (msdTopBit - mantissaTopBit));
        mantissa = 0;
    }

    if (xMSD > compareMantissa)
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    if (xMSD < compareMantissa)
        return xSign ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
    
    // Then, compare additional digits against any remaining mantissa bits.
    for (int digitIndex = xLength - 2; digitIndex >= 0; digitIndex--) {
        if (remainingMantissaBits > 0) {
            remainingMantissaBits -= digitBits;
            if (sizeof(mantissa) != sizeof(xMSD)) {
                compareMantissa = static_cast<Digit>(mantissa >> (64 - digitBits));
                // "& 63" to appease compilers. digitBits is 32 here anyway.
                mantissa = mantissa << (digitBits & 63);
            } else {
                compareMantissa = static_cast<Digit>(mantissa);
                mantissa = 0;
            }
        } else
            compareMantissa = 0;

        Digit digit = x.digit(digitIndex);
        if (digit > compareMantissa)
            return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
        if (digit < compareMantissa)
            return xSign ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
    }

    // Integer parts are equal; check whether {y} has a fractional part.
    if (mantissa) {
        ASSERT(remainingMantissaBits > 0);
        return xSign ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
    }

    return ComparisonResult::Equal;
}

JSBigInt::ComparisonResult JSBigInt::compareToDouble(int32_t x, double y)
{
    return compareToDouble(Int32BigIntImpl { x }, y);
}

JSBigInt::ComparisonResult JSBigInt::compareToDouble(int64_t x, double y)
{
    return compareToDouble(Int64BigIntImpl { x }, y);
}

JSBigInt::ComparisonResult JSBigInt::compareToDouble(uint64_t x, double y)
{
    return compareToDouble(Int64BigIntImpl { x }, y);
}

JSBigInt::ComparisonResult JSBigInt::compareToDouble(JSValue x, double y)
{
    ASSERT(x.isBigInt());
#if USE(BIGINT32)
    if (x.isBigInt32())
        return compareToDouble(x.bigInt32AsInt32(), y);
#endif
    return compareToDouble(x.asHeapBigInt(), y);
}

template <typename BigIntImpl>
std::optional<JSBigInt::Digit> JSBigInt::toShiftAmount(BigIntImpl x)
{
    if (x.length() > 1)
        return std::nullopt;
    
    Digit value = x.digit(0);
    static_assert(maxLengthBits < std::numeric_limits<Digit>::max(), "maxLengthBits needs to be less than digit");
    
    if (value > maxLengthBits)
        return std::nullopt;

    return value;
}

JSBigInt::RoundingResult JSBigInt::decideRounding(JSBigInt* bigInt, int32_t mantissaBitsUnset, int32_t digitIndex, uint64_t currentDigit)
{
    if (mantissaBitsUnset > 0)
        return RoundingResult::RoundDown;
    int32_t topUnconsumedBit = 0;
    if (mantissaBitsUnset < 0) {
        // There are unconsumed bits in currentDigit.
        topUnconsumedBit = -mantissaBitsUnset - 1;
    } else {
        ASSERT(mantissaBitsUnset == 0);
        // currentDigit fit the mantissa exactly; look at the next digit.
        if (digitIndex == 0)
            return RoundingResult::RoundDown;
        digitIndex--;
        currentDigit = static_cast<uint64_t>(bigInt->digit(digitIndex));
        topUnconsumedBit = digitBits - 1;
    }
    // If the most significant remaining bit is 0, round down.
    uint64_t bitmask = static_cast<uint64_t>(1) << topUnconsumedBit;
    if ((currentDigit & bitmask) == 0)
        return RoundingResult::RoundDown;
    // If any other remaining bit is set, round up.
    bitmask -= 1;
    if ((currentDigit & bitmask) != 0)
        return RoundingResult::RoundUp;
    while (digitIndex > 0) {
        digitIndex--;
        if (bigInt->digit(digitIndex) != 0)
            return RoundingResult::RoundUp;
    }
    return RoundingResult::Tie;
}

JSValue JSBigInt::toNumberHeap(JSBigInt* bigInt)
{
    if (bigInt->isZero())
        return jsNumber(0);
    ASSERT(bigInt->length());

    // Conversion mechanism is the following.
    //
    // 1. Get exponent bits.
    // 2. Collect mantissa 52 bits.
    // 3. Add rounding result of unused bits to mantissa and adjust mantissa & exponent bits.
    // 4. Generate double by combining (1) and (3).

    const unsigned length = bigInt->length();
    const bool sign = bigInt->sign();
    const Digit msd = bigInt->digit(length - 1);
    const unsigned msdLeadingZeros = clz(msd);
    const size_t bitLength = length * digitBits - msdLeadingZeros;
    // Double's exponent bits overflow.
    if (bitLength > 1024)
        return jsDoubleNumber(sign ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
    uint64_t exponent = bitLength - 1;
    uint64_t currentDigit = msd;
    int32_t digitIndex = length - 1;
    int32_t shiftAmount = msdLeadingZeros + 1 + (64 - digitBits);
    ASSERT(1 <= shiftAmount);
    ASSERT(shiftAmount <= 64);
    uint64_t mantissa = (shiftAmount == 64) ? 0 : currentDigit << shiftAmount;

    // unsetBits = 64 - setBits - 12 // 12 for non-mantissa bits
    //     setBits = 64 - (msdLeadingZeros + 1 + bitsNotAvailableDueToDigitSize);  // 1 for hidden mantissa bit.
    //                 = 64 - (msdLeadingZeros + 1 + (64 - digitBits))
    //                 = 64 - shiftAmount
    // Hence, unsetBits = 64 - (64 - shiftAmount) - 12 = shiftAmount - 12

    mantissa >>= 12; // (12 = 64 - 52), we shift 12 bits to put 12 zeros in uint64_t mantissa.
    int32_t mantissaBitsUnset = shiftAmount - 12;

    // If not all mantissa bits are defined yet, get more digits as needed.
    // Collect mantissa 52bits from several digits.

    if constexpr (digitBits < 64) {
        if (mantissaBitsUnset >= static_cast<int32_t>(digitBits) && digitIndex > 0) {
            digitIndex--;
            currentDigit = static_cast<uint64_t>(bigInt->digit(digitIndex));
            mantissa |= (currentDigit << (mantissaBitsUnset - digitBits));
            mantissaBitsUnset -= digitBits;
        }
    }

    if (mantissaBitsUnset > 0 && digitIndex > 0) {
        ASSERT(mantissaBitsUnset < static_cast<int32_t>(digitBits));
        digitIndex--;
        currentDigit = static_cast<uint64_t>(bigInt->digit(digitIndex));
        mantissa |= (currentDigit >> (digitBits - mantissaBitsUnset));
        mantissaBitsUnset -= digitBits;
    }

    // If there are unconsumed digits left, we may have to round.
    RoundingResult rounding = decideRounding(bigInt, mantissaBitsUnset, digitIndex, currentDigit);
    if (rounding == RoundingResult::RoundUp || (rounding == RoundingResult::Tie && (mantissa & 1) == 1)) {
        ++mantissa;
        // Incrementing the mantissa can overflow the mantissa bits. In that case the new mantissa will be all zero (plus hidden bit).
        if ((mantissa >> doublePhysicalMantissaSize) != 0) {
            mantissa = 0;
            exponent++;
            // Incrementing the exponent can overflow too.
            if (exponent > 1023)
                return jsDoubleNumber(sign ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
        }
    }

    uint64_t signBit = sign ? (static_cast<uint64_t>(1) << 63) : 0;
    exponent = (exponent + 0x3ff) << doublePhysicalMantissaSize; // 0x3ff is double exponent bias.
    uint64_t doubleBits = signBit | exponent | mantissa;
    ASSERT((doubleBits & (static_cast<uint64_t>(1) << 63)) == signBit);
    ASSERT((doubleBits & (static_cast<uint64_t>(0x7ff) << 52)) == exponent);
    ASSERT((doubleBits & ((static_cast<uint64_t>(1) << 52) - 1)) == mantissa);
    return jsNumber(std::bit_cast<double>(doubleBits));
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::asIntNImpl(JSGlobalObject* globalObject, uint64_t n, BigIntImpl bigInt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (bigInt.isZero())
        return { bigInt };
    if (n == 0)
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    uint64_t neededLength = (n + digitBits - 1) / digitBits;
    uint64_t length = static_cast<uint64_t>(bigInt.length());
    // If bigInt has less than n bits, return it directly.
    if (length < neededLength)
        return { bigInt };
    ASSERT(neededLength <= INT32_MAX);
    Digit topDigit = bigInt.digit(static_cast<int32_t>(neededLength) - 1);
    Digit compareDigit = static_cast<Digit>(1) << ((n - 1) % digitBits);
    if (length == neededLength && topDigit < compareDigit)
        return { bigInt };

    // Otherwise we have to truncate (which is a no-op in the special case
    // of bigInt == -2^(n-1)), and determine the right sign. We also might have
    // to subtract from 2^n to simulate having two's complement representation.
    // In most cases, the result's sign is bigInt.sign() xor "(n-1)th bit present".
    // The only exception is when bigInt is negative, has the (n-1)th bit, and all
    // its bits below (n-1) are zero. In that case, the result is the minimum
    // n-bit integer (example: asIntN(3, -12n) => -4n).
    bool hasBit = (topDigit & compareDigit) == compareDigit;
    ASSERT(n <= INT32_MAX);
    int32_t N = static_cast<int32_t>(n);
    if (!hasBit)
        RELEASE_AND_RETURN(scope, truncateToNBits(globalObject, N, bigInt));
    if (!bigInt.sign())
        RELEASE_AND_RETURN(scope, truncateAndSubFromPowerOfTwo(globalObject, N, bigInt, true));

    // Negative numbers must subtract from 2^n, except for the special case
    // described above.
    if ((topDigit & (compareDigit - 1)) == 0) {
        for (int32_t i = static_cast<int32_t>(neededLength) - 2; i >= 0; i--) {
            if (bigInt.digit(i) != 0)
                RELEASE_AND_RETURN(scope, truncateAndSubFromPowerOfTwo(globalObject, N, bigInt, false));
        }
        // Truncation is no-op if bigInt == -2^(n-1).
        if (length == neededLength && topDigit == compareDigit)
            return { bigInt };
        RELEASE_AND_RETURN(scope, truncateToNBits(globalObject, N, bigInt));
    }
    RELEASE_AND_RETURN(scope, truncateAndSubFromPowerOfTwo(globalObject, N, bigInt, false));
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::asUintNImpl(JSGlobalObject* globalObject, uint64_t n, BigIntImpl bigInt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (bigInt.isZero())
        return { bigInt };
    if (n == 0)
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    // If bigInt is negative, simulate two's complement representation.
    if (bigInt.sign()) {
        if (n > maxLengthBits) {
            throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
            return nullptr;
        }
        RELEASE_AND_RETURN(scope, truncateAndSubFromPowerOfTwo(globalObject, static_cast<int32_t>(n), bigInt, false));
    }

    // If bigInt is positive and has up to n bits, return it directly.
    if (n >= maxLengthBits)
        return { bigInt };
    static_assert(maxLengthBits < INT32_MAX - digitBits);
    int32_t neededLength = static_cast<int32_t>((n + digitBits - 1) / digitBits);
    if (static_cast<int32_t>(bigInt.length()) < neededLength)
        return { bigInt };

    int32_t bitsInTopDigit = n % digitBits;
    if (static_cast<int32_t>(bigInt.length()) == neededLength) {
        if (bitsInTopDigit == 0)
            return { bigInt };
        Digit topDigit = bigInt.digit(neededLength - 1);
        if ((topDigit >> bitsInTopDigit) == 0)
            return { bigInt };
    }

    // Otherwise, truncate.
    ASSERT(n <= INT32_MAX);
    RELEASE_AND_RETURN(scope, truncateToNBits(globalObject, static_cast<int32_t>(n), bigInt));
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::truncateToNBits(JSGlobalObject* globalObject, int32_t n, BigIntImpl bigInt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto span = bigInt.digits();

    ASSERT(n != 0);
    ASSERT(span.size() > n / digitBits);

    int32_t neededDigits = (n + (digitBits - 1)) / digitBits;
    ASSERT(neededDigits <= static_cast<int32_t>(span.size()));

    Vector<Digit, 16> resultVector(neededDigits);
    auto result = resultVector.mutableSpan();

    // Copy all digits except the MSD.
    int32_t last = neededDigits - 1;
    for (int32_t i = 0; i < last; i++)
        result[i] = span[i];

    // The MSD might contain extra bits that we don't want.
    Digit msd = span[last];
    if (n % digitBits != 0) {
        int32_t drop = digitBits - (n % digitBits);
        msd = (msd << drop) >> drop;
    }
    result[last] = msd;
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, bigInt.sign(), result));
}

// Subtracts the least significant n bits of abs(bigInt) from 2^n.
template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::truncateAndSubFromPowerOfTwo(JSGlobalObject* globalObject, int32_t n, BigIntImpl bigInt, bool resultSign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(n != 0);
    ASSERT(n <= static_cast<int32_t>(maxLengthBits));

    auto span = bigInt.digits();

    int32_t neededDigits = (n + (digitBits - 1)) / digitBits;
    ASSERT(neededDigits <= static_cast<int32_t>(maxLength)); // Follows from n <= maxLengthBits.

    Vector<Digit, 16> resultVector(neededDigits);
    auto result = resultVector.mutableSpan();

    // Process all digits except the MSD.
    int32_t i = 0;
    int32_t last = neededDigits - 1;
    int32_t length = span.size();
    Digit borrow = 0;
    // Take digits from bigInt unless its length is exhausted.
    int32_t limit = std::min(last, length);
    for (; i < limit; i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(0, span[i], newBorrow);
        difference = digitSub(difference, borrow, newBorrow);
        result[i] = difference;
        borrow = newBorrow;
    }
    // Then simulate leading zeroes in {bigInt} as needed.
    for (; i < last; i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(0, borrow, newBorrow);
        result[i] = difference;
        borrow = newBorrow;
    }

    // The MSD might contain extra bits that we don't want.
    Digit msd = last < length ? span[last] : 0;
    int32_t msdBitsConsumed = n % digitBits;
    Digit resultMSD;
    if (msdBitsConsumed == 0) {
        Digit newBorrow = 0;
        resultMSD = digitSub(0, msd, newBorrow);
        resultMSD = digitSub(resultMSD, borrow, newBorrow);
    } else {
        int32_t drop = digitBits - msdBitsConsumed;
        msd = (msd << drop) >> drop;
        Digit minuendMSD = static_cast<Digit>(1) << (digitBits - drop);
        Digit newBorrow = 0;
        resultMSD = digitSub(minuendMSD, msd, newBorrow);
        resultMSD = digitSub(resultMSD, borrow, newBorrow);
        ASSERT(newBorrow == 0); // result < 2^n.
        // If all subtracted bits were zero, we have to get rid of the
        // materialized minuendMSD again.
        resultMSD &= (minuendMSD - 1);
    }
    result[last] = resultMSD;
    RELEASE_AND_RETURN(scope, tryCreateFromImpl(globalObject, vm, resultSign, result));
}

JSValue JSBigInt::asIntN(JSGlobalObject* globalObject, uint64_t n, JSBigInt* bigInt)
{
    return tryConvertToBigInt32(asIntNImpl(globalObject, n, HeapBigIntImpl { bigInt }));
}

JSValue JSBigInt::asUintN(JSGlobalObject* globalObject, uint64_t n, JSBigInt* bigInt)
{
    return tryConvertToBigInt32(asUintNImpl(globalObject, n, HeapBigIntImpl { bigInt }));
}

#if USE(BIGINT32)
JSValue JSBigInt::asIntN(JSGlobalObject* globalObject, uint64_t n, int32_t bigInt)
{
    return tryConvertToBigInt32(asIntNImpl(globalObject, n, Int32BigIntImpl { bigInt }));
}

JSValue JSBigInt::asUintN(JSGlobalObject* globalObject, uint64_t n, int32_t bigInt)
{
    return tryConvertToBigInt32(asUintNImpl(globalObject, n, Int32BigIntImpl { bigInt }));
}
#endif

uint64_t JSBigInt::toBigUInt64Heap(JSBigInt* bigInt)
{
    auto length = bigInt->length();
    if (!length)
        return 0;
    uint64_t value = 0;
    if constexpr (sizeof(Digit) == 4) {
        value = static_cast<uint64_t>(bigInt->digit(0));
        if (length > 1)
            value |= static_cast<uint64_t>(bigInt->digit(1)) << 32;
    } else {
        ASSERT(sizeof(Digit) == 8);
        value = bigInt->digit(0);
    }
    if (!bigInt->sign())
        return value;
    return ~(value - 1); // To avoid undefined behavior, we compute two's compliment by hand in C while this is simply `-value`.
}

static ALWAYS_INLINE unsigned computeHash(JSBigInt::Digit* digits, unsigned length, bool sign)
{
    Hasher hasher;
    WTF::add(hasher, sign);
    for (unsigned index = 0; index < length; ++index)
        WTF::add(hasher, digits[index]);
    return hasher.hash();
}

std::optional<unsigned> JSBigInt::concurrentHash()
{
    // FIXME: Implement JSBigInt::concurrentHash by inserting right store barriers.
    // https://bugs.webkit.org/show_bug.cgi?id=216801
    return std::nullopt;
}

unsigned JSBigInt::hashSlow()
{
    ASSERT(!m_hash);
    m_hash = computeHash(dataStorage(), length(), sign());
    return m_hash;
}

JSBigInt* JSBigInt::tryCreateFromImpl(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, bool sign, std::span<const Digit> digits)
{
    digits = normalize(digits);
    if (digits.empty())
        return createZero(nullOrGlobalObjectForOOM, vm);

    JSBigInt* result = createWithLength(nullOrGlobalObjectForOOM, vm, digits.size());
    if (!result) [[unlikely]]
        return nullptr;
    memcpySpan(result->digits(), digits);
    result->setSign(sign);
    return result;
}

JSBigInt* JSBigInt::tryCreateFrom(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, bool sign, std::span<const Digit> digits)
{
    return tryCreateFromImpl(nullOrGlobalObjectForOOM, vm, sign, digits);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
