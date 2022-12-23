/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
 * Copyright 2017 the V8 project authors. All rights reserved.
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

namespace JSC {

const ClassInfo JSBigInt::s_info = { "BigInt"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSBigInt) };

JSBigInt::JSBigInt(VM& vm, Structure* structure, Digit* data, unsigned length)
    : Base(vm, structure)
    , m_length(length)
    , m_data(vm, this, data, length)
{ }

template<typename Visitor>
void JSBigInt::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSBigInt*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    if (auto* data = thisObject->m_data.getUnsafe())
        visitor.markAuxiliary(data);
}

DEFINE_VISIT_CHILDREN(JSBigInt);

void JSBigInt::initialize(InitializationType initType)
{
    if (initType == InitializationType::WithZero)
        memset(dataStorage(), 0, length() * sizeof(Digit));
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
    if (UNLIKELY(length > maxLength)) {
        if (nullOrGlobalObjectForOOM) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope, "BigInt generated from this operation is too big"_s);
        }
        return nullptr;
    }

    ASSERT(length <= maxLength);
    void* data = vm.primitiveGigacageAuxiliarySpace().allocate(vm, length * sizeof(Digit), nullptr, AllocationFailureMode::ReturnNull);
    if (UNLIKELY(!data)) {
        if (nullOrGlobalObjectForOOM) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
        }
        return nullptr;
    }
    JSBigInt* bigInt = new (NotNull, allocateCell<JSBigInt>(vm)) JSBigInt(vm, vm.bigIntStructure.get(), reinterpret_cast<Digit*>(data), length);
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
    if (UNLIKELY(!bigInt))
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

inline JSBigInt* JSBigInt::createFromImpl(JSGlobalObject* globalObject, uint64_t value, bool sign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value)
        RELEASE_AND_RETURN(scope, createZero(globalObject));

    // This path is not just an optimization: because we do not call rightTrim at the end of this function,
    // it would be a bug to create a BigInt with length=2 in this case.
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
    return createFromImpl(globalObject, value, false);
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
    return createFromImpl(globalObject, unsignedValue, sign);
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
        RELEASE_AND_RETURN(scope, createFromImpl(globalObject, static_cast<uint64_t>(unsignedValue), sign));

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
    uint64_t doubleBits = bitwise_cast<uint64_t>(value);
    int32_t rawExponent = static_cast<int32_t>(doubleBits >> doublePhysicalMantissaSize) & 0x7ff;
    ASSERT(rawExponent != 0x7ff); // Since value is integer, exponent should not be 0x7ff (full bits, used for infinity etc.).
    ASSERT(rawExponent >= 0x3ff); // Since value is integer, exponent should be >= 0 + bias (0x3ff).
    int32_t exponent = rawExponent - 0x3ff;
    int32_t digits = exponent / digitBits + 1;
    JSBigInt* result = createWithLength(globalObject, digits);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result);
    result->initialize(InitializationType::WithZero);
    result->setSign(sign);

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
    result->setDigit(digits - 1, digit);
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
        result->setDigit(digitIndex, digit);
    }
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

JSValue JSBigInt::toPrimitive(JSGlobalObject*, PreferredPrimitiveType) const
{
    return const_cast<JSBigInt*>(this);
}

JSValue JSBigInt::parseInt(JSGlobalObject* globalObject, StringView s, ErrorParseMode parserMode)
{
    if (s.is8Bit())
        return parseInt(globalObject, s.characters8(), s.length(), parserMode);
    return parseInt(globalObject, s.characters16(), s.length(), parserMode);
}

JSValue JSBigInt::parseInt(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, StringView s, uint8_t radix, ErrorParseMode parserMode, ParseIntSign sign)
{
    if (s.is8Bit())
        return parseInt(nullOrGlobalObjectForOOM, vm, s.characters8(), s.length(), 0, radix, parserMode, sign, ParseIntMode::DisallowEmptyString);
    return parseInt(nullOrGlobalObjectForOOM, vm, s.characters16(), s.length(), 0, radix, parserMode, sign, ParseIntMode::DisallowEmptyString);
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
    ALWAYS_INLINE JSBigInt* toHeapBigInt(JSGlobalObject*, VM&) { return m_bigInt; }
    ALWAYS_INLINE JSBigInt* toHeapBigInt(JSGlobalObject*) { return m_bigInt; }

private:
    friend struct JSBigInt::ImplResult;
    JSBigInt* m_bigInt;
};

class Int32BigIntImpl {
public:
    explicit Int32BigIntImpl(int32_t value)
        : m_value(value)
    { }

    ALWAYS_INLINE bool isZero() { return !m_value; }
    ALWAYS_INLINE bool sign() { return m_value < 0; }
    ALWAYS_INLINE unsigned length() { return isZero() ? 0 : 1; }
    ALWAYS_INLINE JSBigInt::Digit digit(unsigned i)
    {
        ASSERT(length());
        ASSERT_UNUSED(i, i == 0);
        if (sign())
            return static_cast<JSBigInt::Digit>(-static_cast<int64_t>(m_value));
        return m_value;
    }

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

// Multiplies {this} with {factor} and adds {summand} to the result.
void JSBigInt::inplaceMultiplyAdd(Digit factor, Digit summand)
{
    internalMultiplyAdd(HeapBigIntImpl { this }, factor, summand, length(), this);
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
        return base;

    if (base.length() == 1 && base.digit(0) == 1) {
        // (-1) ** even_number == 1.
        if (base.sign() && !(exponent.digit(0) & 1))
            RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, base));

        // (-1) ** odd_number == -1; 1 ** anything == 1.
        return base;
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
        return base;
    if (expValue >= maxLengthBits) {
        throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
        return nullptr;
    }

    static_assert(maxLengthBits <= maxInt, "maxLengthBits needs to be <= maxInt");
    int n = static_cast<int>(expValue);
    if (base.length() == 1 && base.digit(0) == 2) {
        // Fast path for 2^n.
        int neededDigits = 1 + (n / digitBits);
        JSBigInt* result = JSBigInt::createWithLength(globalObject, neededDigits);
        RETURN_IF_EXCEPTION(scope, nullptr);

        result->initialize(InitializationType::WithZero);
        // All bits are zero. Now set the n-th bit.
        Digit msd = static_cast<Digit>(1) << (n % digitBits);
        result->setDigit(neededDigits - 1, msd);
        // Result is negative for odd powers of -2n.
        if (base.sign()) 
            result->setSign(static_cast<bool>(n & 1));

        return result;
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

    return result;
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

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::multiplyImpl(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.isZero())
        return x;
    if (y.isZero())
        return y;

    unsigned resultLength = x.length() + y.length();
    JSBigInt* result = JSBigInt::createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, nullptr);
    result->initialize(InitializationType::WithZero);

    for (unsigned i = 0; i < x.length(); i++)
        multiplyAccumulate(y, x.digit(i), result, i);

    result->setSign(x.sign() != y.sign());
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
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
    if (absoluteCompare(x, y) == ComparisonResult::LessThan)
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    JSBigInt* quotient = nullptr;
    bool resultSign = x.sign() != y.sign();
    if (y.length() == 1) {
        Digit divisor = y.digit(0);
        if (divisor == 1) {
            if (resultSign == x.sign())
                return JSBigInt::ImplResult { x };
            RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, x));
        }

        Digit remainder;
        absoluteDivWithDigitDivisor(globalObject, vm, x, divisor, &quotient, remainder);
        RETURN_IF_EXCEPTION(scope, nullptr);
    } else {
        JSBigInt* yBigInt = y.toHeapBigInt(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        absoluteDivWithBigIntDivisor(globalObject, x, yBigInt, &quotient, nullptr);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    quotient->setSign(resultSign);
    RELEASE_AND_RETURN(scope, quotient->rightTrim(globalObject));
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

    for (unsigned i = 0; i < result->length(); ++i)
        result->setDigit(i, x.digit(i));
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
    if (absoluteCompare(x, y) == ComparisonResult::LessThan)
        return x;

    JSBigInt* remainder;
    if (y.length() == 1) {
        Digit divisor = y.digit(0);
        if (divisor == 1)
            RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

        Digit remainderDigit;
        absoluteDivWithDigitDivisor(globalObject, vm, x, divisor, nullptr, remainderDigit);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (!remainderDigit)
            RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

        remainder = createWithLength(globalObject, 1);
        RETURN_IF_EXCEPTION(scope, nullptr);
        remainder->setDigit(0, remainderDigit);
    } else {
        JSBigInt* yBigInt = y.toHeapBigInt(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        absoluteDivWithBigIntDivisor(globalObject, x, yBigInt, nullptr, &remainder);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    remainder->setSign(x.sign());
    RELEASE_AND_RETURN(scope, remainder->rightTrim(globalObject));
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

    if (!x.sign())
        RELEASE_AND_RETURN(scope, absoluteAddOne(globalObject, x, SignOption::Unsigned));
    JSBigInt* result = absoluteSubOne(globalObject, x, x.length());
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (result->isZero())
        return result;
    result->setSign(true);
    return result;
}

JSValue JSBigInt::inc(JSGlobalObject* globalObject, JSBigInt* x)
{
    return tryConvertToBigInt32(incImpl(globalObject, HeapBigIntImpl { x }));
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::decImpl(JSGlobalObject* globalObject, BigIntImpl x)
{
    if (x.isZero()) {
#if USE(BIGINT32)
        return jsBigInt32(-1);
#else
        return createFrom(globalObject, -1);
#endif
    }
    if (!x.sign())
        return absoluteSubOne(globalObject, x, x.length());
    return absoluteAddOne(globalObject, x, SignOption::Signed);
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

    if (!x.sign() && !y.sign())
        RELEASE_AND_RETURN(scope, absoluteAnd(globalObject, x, y));
    
    if (x.sign() && y.sign()) {
        int resultLength = std::max(x.length(), y.length()) + 1;
        // (-x) & (-y) == ~(x-1) & ~(y-1) == ~((x-1) | (y-1))
        // == -(((x-1) | (y-1)) + 1)
        JSBigInt* result = absoluteSubOne(globalObject, x, resultLength);
        RETURN_IF_EXCEPTION(scope, nullptr);

        JSBigInt* y1 = absoluteSubOne(globalObject, y, y.length());
        RETURN_IF_EXCEPTION(scope, nullptr);
        result = absoluteOr(globalObject, HeapBigIntImpl { result }, HeapBigIntImpl { y1 });
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, absoluteAddOne(globalObject, HeapBigIntImpl { result }, SignOption::Signed));
    }

    ASSERT(x.sign() != y.sign());
    // x & (-y) == x & ~(y-1)
    auto computeResult = [&] (auto x, auto y) -> JSBigInt* {
        ASSERT(!x.sign()); 
        ASSERT(y.sign()); 
        JSBigInt* y1 = absoluteSubOne(globalObject, y, y.length());
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, absoluteAndNot(globalObject, x, HeapBigIntImpl { y1 }));
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

    unsigned resultLength = std::max(x.length(), y.length());

    if (!x.sign() && !y.sign())
        RELEASE_AND_RETURN(scope, absoluteOr(globalObject, x, y));
    
    if (x.sign() && y.sign()) {
        // (-x) | (-y) == ~(x-1) | ~(y-1) == ~((x-1) & (y-1))
        // == -(((x-1) & (y-1)) + 1)
        JSBigInt* result = absoluteSubOne(globalObject, x, resultLength);
        RETURN_IF_EXCEPTION(scope, nullptr);
        JSBigInt* y1 = absoluteSubOne(globalObject, y, y.length());
        RETURN_IF_EXCEPTION(scope, nullptr);
        result = absoluteAnd(globalObject, HeapBigIntImpl { result }, HeapBigIntImpl { y1 });
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, absoluteAddOne(globalObject, HeapBigIntImpl { result }, SignOption::Signed));
    }
    
    ASSERT(x.sign() != y.sign());

    // x | (-y) == x | ~(y-1) == ~((y-1) &~ x) == -(((y-1) &~ x) + 1)
    auto computeResult = [&] (auto x, auto y) -> JSBigInt* {
        ASSERT(!x.sign());
        ASSERT(y.sign());

        JSBigInt* result = absoluteSubOne(globalObject, y, resultLength);
        RETURN_IF_EXCEPTION(scope, nullptr);
        result = absoluteAndNot(globalObject, HeapBigIntImpl { result }, x);
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, absoluteAddOne(globalObject, HeapBigIntImpl { result }, SignOption::Signed));
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

    if (!x.sign() && !y.sign())
        RELEASE_AND_RETURN(scope, absoluteXor(globalObject, x, y));
    
    if (x.sign() && y.sign()) {
        int resultLength = std::max(x.length(), y.length());
        
        // (-x) ^ (-y) == ~(x-1) ^ ~(y-1) == (x-1) ^ (y-1)
        JSBigInt* result = absoluteSubOne(globalObject, x, resultLength);
        RETURN_IF_EXCEPTION(scope, nullptr);
        JSBigInt* y1 = absoluteSubOne(globalObject, y, y.length());
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, absoluteXor(globalObject, HeapBigIntImpl { result }, HeapBigIntImpl { y1 }));
    }
    ASSERT(x.sign() != y.sign());
    int resultLength = std::max(x.length(), y.length()) + 1;

    // x ^ (-y) == x ^ ~(y-1) == ~(x ^ (y-1)) == -((x ^ (y-1)) + 1)
    auto computeResult = [&] (auto x, auto y) -> JSBigInt* {
        ASSERT(!x.sign());
        ASSERT(y.sign());
        JSBigInt* result = absoluteSubOne(globalObject, y, resultLength);
        RETURN_IF_EXCEPTION(scope, nullptr);

        result = absoluteXor(globalObject, HeapBigIntImpl { result }, x);
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, absoluteAddOne(globalObject, HeapBigIntImpl { result }, SignOption::Signed));
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
        return x;

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
        return x;

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
    if (x.sign()) {
        // ~(-x) == ~(~(x-1)) == x-1
        return absoluteSubOne(globalObject, x, x.length());
    } 
    // ~x == -x-1 == -(x+1)
    return absoluteAddOne(globalObject, x, SignOption::Signed);
}

JSValue JSBigInt::bitwiseNot(JSGlobalObject* globalObject, JSBigInt* x)
{
    return tryConvertToBigInt32(bitwiseNotImpl(globalObject, HeapBigIntImpl { x }));
}

#if USE(JSVALUE32_64)
using TwoDigit = uint64_t;
#else
using TwoDigit = UInt128;
#endif

// {carry} must point to an initialized Digit and will either be incremented
// by one or left alone.
inline JSBigInt::Digit JSBigInt::digitAdd(Digit a, Digit b, Digit& carry)
{
    Digit result = a + b;
    carry += static_cast<bool>(result < a);
    return result;
}

// {borrow} must point to an initialized Digit and will either be incremented
// by one or left alone.
inline JSBigInt::Digit JSBigInt::digitSub(Digit a, Digit b, Digit& borrow)
{
    Digit result = a - b;
    borrow += static_cast<bool>(result > a);
    return result;
}

// Returns the low half of the result. High half is in {high}.
inline JSBigInt::Digit JSBigInt::digitMul(Digit a, Digit b, Digit& high)
{
    TwoDigit result = static_cast<TwoDigit>(a) * static_cast<TwoDigit>(b);
    high = static_cast<Digit>(result >> digitBits);
    return static_cast<Digit>(result);
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
#if CPU(X86_64) && COMPILER(GCC_COMPATIBLE)
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
#elif CPU(X86) && COMPILER(GCC_COMPATIBLE)
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
template <typename BigIntImpl>
void JSBigInt::internalMultiplyAdd(BigIntImpl source, Digit factor, Digit summand, unsigned n, JSBigInt* result)
{
    ASSERT(source.length() >= n);
    ASSERT(result->length() >= n);

    Digit carry = summand;
    Digit high = 0;
    for (unsigned i = 0; i < n; i++) {
        Digit current = source.digit(i);
        Digit newCarry = 0;

        // Compute this round's multiplication.
        Digit newHigh = 0;
        current = digitMul(current, factor, newHigh);

        // Add last round's carryovers.
        current = digitAdd(current, high, newCarry);
        current = digitAdd(current, carry, newCarry);

        // Store result and prepare for next round.
        result->setDigit(i, current);
        carry = newCarry;
        high = newHigh;
    }

    if (result->length() > n) {
        result->setDigit(n++, carry + high);

        // Current callers don't pass in such large results, but let's be robust.
        while (n < result->length())
            result->setDigit(n++, 0);
    } else
        ASSERT(!(carry + high));
}

// Multiplies {multiplicand} with {multiplier} and adds the result to
// {accumulator}, starting at {accumulatorIndex} for the least-significant
// digit.
// Callers must ensure that {accumulator} is big enough to hold the result.
template <typename BigIntImpl>
void JSBigInt::multiplyAccumulate(BigIntImpl multiplicand, Digit multiplier, JSBigInt* accumulator, unsigned accumulatorIndex)
{
    ASSERT(accumulator->length() > multiplicand.length() + accumulatorIndex);
    if (!multiplier)
        return;
    
    Digit carry = 0;
    Digit high = 0;
    for (unsigned i = 0; i < multiplicand.length(); i++, accumulatorIndex++) {
        Digit acc = accumulator->digit(accumulatorIndex);
        Digit newCarry = 0;
        
        // Add last round's carryovers.
        acc = digitAdd(acc, high, newCarry);
        acc = digitAdd(acc, carry, newCarry);
        
        // Compute this round's multiplication.
        Digit multiplicandDigit = multiplicand.digit(i);
        Digit low = digitMul(multiplier, multiplicandDigit, high);
        acc = digitAdd(acc, low, newCarry);
        
        // Store result and prepare for next round.
        accumulator->setDigit(accumulatorIndex, acc);
        carry = newCarry;
    }
    
    while (carry || high) {
        ASSERT(accumulatorIndex < accumulator->length());
        Digit acc = accumulator->digit(accumulatorIndex);
        Digit newCarry = 0;
        acc = digitAdd(acc, high, newCarry);
        high = 0;
        acc = digitAdd(acc, carry, newCarry);
        accumulator->setDigit(accumulatorIndex, acc);
        carry = newCarry;
        accumulatorIndex++;
    }
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

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::absoluteAdd(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y, bool resultSign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (x.length() < y.length())
        RELEASE_AND_RETURN(scope, absoluteAdd(globalObject, y, x, resultSign));

    if (x.isZero()) {
        ASSERT(y.isZero());
        return x;
    }

    if (y.isZero()) {
        if (resultSign == x.sign())
            return x;
        RELEASE_AND_RETURN(scope, unaryMinusImpl(globalObject, x));
    }

    JSBigInt* result = createWithLength(globalObject, x.length() + 1);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result);
    Digit carry = 0;
    unsigned i = 0;
    for (; i < y.length(); i++) {
        Digit newCarry = 0;
        Digit sum = digitAdd(x.digit(i), y.digit(i), newCarry);
        sum = digitAdd(sum, carry, newCarry);
        result->setDigit(i, sum);
        carry = newCarry;
    }

    for (; i < x.length(); i++) {
        Digit newCarry = 0;
        Digit sum = digitAdd(x.digit(i), carry, newCarry);
        result->setDigit(i, sum);
        carry = newCarry;
    }

    result->setDigit(i, carry);
    result->setSign(resultSign);

    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
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
        return x;
    }

    if (y.isZero()) {
        if (resultSign == x.sign())
            return ImplResult { x };
        RELEASE_AND_RETURN(scope, JSBigInt::unaryMinusImpl(globalObject, x));
    }

    if (comparisonResult == ComparisonResult::Equal)
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    JSBigInt* result = createWithLength(globalObject, x.length());
    RETURN_IF_EXCEPTION(scope, nullptr);

    Digit borrow = 0;
    unsigned i = 0;
    for (; i < y.length(); i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(x.digit(i), y.digit(i), newBorrow);
        difference = digitSub(difference, borrow, newBorrow);
        result->setDigit(i, difference);
        borrow = newBorrow;
    }

    for (; i < x.length(); i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(x.digit(i), borrow, newBorrow);
        result->setDigit(i, difference);
        borrow = newBorrow;
    }

    ASSERT(!borrow);
    result->setSign(resultSign);
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

// Divides {x} by {divisor}, returning the result in {quotient} and {remainder}.
// Mathematically, the contract is:
// quotient = (x - remainder) / divisor, with 0 <= remainder < divisor.
// If {quotient} is an empty handle, an appropriately sized BigInt will be
// allocated for it; otherwise the caller must ensure that it is big enough.
// {quotient} can be the same as {x} for an in-place division. {quotient} can
// also be nullptr if the caller is only interested in the remainder.
template <typename BigIntImpl>
bool JSBigInt::absoluteDivWithDigitDivisor(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, BigIntImpl x, Digit divisor, JSBigInt** quotient, Digit& remainder)
{
    ASSERT(divisor);

    ASSERT(!x.isZero());
    remainder = 0;
    if (divisor == 1) {
        if (quotient) {
            JSBigInt* result = x.toHeapBigInt(nullOrGlobalObjectForOOM, vm);
            if (UNLIKELY(!result))
                return false;
            *quotient = result;
        }
        return true;
    }

    unsigned length = x.length();
    if (quotient) {
        if (*quotient == nullptr) {
            JSBigInt* result = createWithLength(nullOrGlobalObjectForOOM, vm, length);
            if (UNLIKELY(!result))
                return false;
            *quotient = result;
        }

        for (int i = length - 1; i >= 0; i--) {
            Digit q = digitDiv(remainder, x.digit(i), divisor, remainder);
            (*quotient)->setDigit(i, q);
        }
    } else {
        for (int i = length - 1; i >= 0; i--)
            digitDiv(remainder, x.digit(i), divisor, remainder);
    }
    return true;
}

// Divides {dividend} by {divisor}, returning the result in {quotient} and
// {remainder}. Mathematically, the contract is:
// quotient = (dividend - remainder) / divisor, with 0 <= remainder < divisor.
// Both {quotient} and {remainder} are optional, for callers that are only
// interested in one of them.
// See Knuth, Volume 2, section 4.3.1, Algorithm D.
template <typename BigIntImpl1>
void JSBigInt::absoluteDivWithBigIntDivisor(JSGlobalObject* globalObject, BigIntImpl1 dividend, JSBigInt* divisor, JSBigInt** quotient, JSBigInt** remainder)
{
    ASSERT(divisor->length() >= 2);
    ASSERT(dividend.length() >= divisor->length());
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // The unusual variable names inside this function are consistent with
    // Knuth's book, as well as with Go's implementation of this algorithm.
    // Maintaining this consistency is probably more useful than trying to
    // come up with more descriptive names for them.
    unsigned n = divisor->length();
    unsigned m = dividend.length() - n;
    
    // The quotient to be computed.
    JSBigInt* q = nullptr;
    if (quotient != nullptr) {
        q = createWithLength(globalObject, m + 1);
        RETURN_IF_EXCEPTION(scope, void());
    }
    
    // In each iteration, {qhatv} holds {divisor} * {current quotient digit}.
    // "v" is the book's name for {divisor}, "qhat" the current quotient digit.
    JSBigInt* qhatv = createWithLength(globalObject, n + 1);
    RETURN_IF_EXCEPTION(scope, void());
    
    // D1.
    // Left-shift inputs so that the divisor's MSB is set. This is necessary
    // to prevent the digit-wise divisions (see digit_div call below) from
    // overflowing (they take a two digits wide input, and return a one digit
    // result).
    Digit lastDigit = divisor->digit(n - 1);
    unsigned shift = clz(lastDigit);

    if (shift > 0) {
        divisor = absoluteLeftShiftAlwaysCopy(globalObject, HeapBigIntImpl { divisor }, shift, LeftShiftMode::SameSizeResult);
        RETURN_IF_EXCEPTION(scope, void());
    }

    // Holds the (continuously updated) remaining part of the dividend, which
    // eventually becomes the remainder.
    JSBigInt* u = absoluteLeftShiftAlwaysCopy(globalObject, dividend, shift, LeftShiftMode::AlwaysAddOneDigit);
    RETURN_IF_EXCEPTION(scope, void());

    // D2.
    // Iterate over the dividend's digit (like the "grad school" algorithm).
    // {vn1} is the divisor's most significant digit.
    Digit vn1 = divisor->digit(n - 1);
    for (int j = m; j >= 0; j--) {
        // D3.
        // Estimate the current iteration's quotient digit (see Knuth for details).
        // {qhat} is the current quotient digit.
        Digit qhat = std::numeric_limits<Digit>::max();

        // {ujn} is the dividend's most significant remaining digit.
        Digit ujn = u->digit(j + n);
        if (ujn != vn1) {
            // {rhat} is the current iteration's remainder.
            Digit rhat = 0;
            // Estimate the current quotient digit by dividing the most significant
            // digits of dividend and divisor. The result will not be too small,
            // but could be a bit too large.
            qhat = digitDiv(ujn, u->digit(j + n - 1), vn1, rhat);
            
            // Decrement the quotient estimate as needed by looking at the next
            // digit, i.e. by testing whether
            // qhat * v_{n-2} > (rhat << digitBits) + u_{j+n-2}.
            Digit vn2 = divisor->digit(n - 2);
            Digit ujn2 = u->digit(j + n - 2);
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
        internalMultiplyAdd(HeapBigIntImpl { divisor }, qhat, 0, n, qhatv);
        Digit c = u->absoluteInplaceSub(qhatv, j);
        if (c) {
            c = u->absoluteInplaceAdd(divisor, j);
            u->setDigit(j + n, u->digit(j + n) + c);
            qhat--;
        }
        
        if (quotient != nullptr)
            q->setDigit(j, qhat);
    }

    if (quotient != nullptr) {
        // Caller will right-trim.
        *quotient = q;
    }

    if (remainder != nullptr) {
        u->inplaceRightShift(shift);
        *remainder = u;
    }
}

// Returns whether (factor1 * factor2) > (high << digitBits) + low.
inline bool JSBigInt::productGreaterThan(Digit factor1, Digit factor2, Digit high, Digit low)
{
    Digit resultHigh;
    Digit resultLow = digitMul(factor1, factor2, resultHigh);
    return resultHigh > high || (resultHigh == high && resultLow > low);
}

// Adds {summand} onto {this}, starting with {summand}'s 0th digit
// at {this}'s {startIndex}'th digit. Returns the "carry" (0 or 1).
JSBigInt::Digit JSBigInt::absoluteInplaceAdd(JSBigInt* summand, unsigned startIndex)
{
    Digit carry = 0;
    unsigned n = summand->length();
    ASSERT(length() >= startIndex + n);
    for (unsigned i = 0; i < n; i++) {
        Digit newCarry = 0;
        Digit sum = digitAdd(digit(startIndex + i), summand->digit(i), newCarry);
        sum = digitAdd(sum, carry, newCarry);
        setDigit(startIndex + i, sum);
        carry = newCarry;
    }

    return carry;
}

// Subtracts {subtrahend} from {this}, starting with {subtrahend}'s 0th digit
// at {this}'s {startIndex}-th digit. Returns the "borrow" (0 or 1).
JSBigInt::Digit JSBigInt::absoluteInplaceSub(JSBigInt* subtrahend, unsigned startIndex)
{
    Digit borrow = 0;
    unsigned n = subtrahend->length();
    ASSERT(length() >= startIndex + n);
    for (unsigned i = 0; i < n; i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(digit(startIndex + i), subtrahend->digit(i), newBorrow);
        difference = digitSub(difference, borrow, newBorrow);
        setDigit(startIndex + i, difference);
        borrow = newBorrow;
    }

    return borrow;
}

void JSBigInt::inplaceRightShift(unsigned shift)
{
    ASSERT(shift < digitBits);
    ASSERT(!(digit(0) & ((static_cast<Digit>(1) << shift) - 1)));

    if (!shift)
        return;

    Digit carry = digit(0) >> shift;
    unsigned last = length() - 1;
    for (unsigned i = 0; i < last; i++) {
        Digit d = digit(i + 1);
        setDigit(i, (d << (digitBits - shift)) | carry);
        carry = d >> shift;
    }
    setDigit(last, carry);
}

// Always copies the input, even when {shift} == 0.
template <typename BigIntImpl>
JSBigInt* JSBigInt::absoluteLeftShiftAlwaysCopy(JSGlobalObject* globalObject, BigIntImpl x, unsigned shift, LeftShiftMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(shift < digitBits);
    ASSERT(!x.isZero());

    unsigned n = x.length();
    unsigned resultLength = mode == LeftShiftMode::AlwaysAddOneDigit ? n + 1 : n;
    JSBigInt* result = createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, { });

    if (!shift) {
        for (unsigned i = 0; i < n; i++)
            result->setDigit(i, x.digit(i));
        if (mode == LeftShiftMode::AlwaysAddOneDigit)
            result->setDigit(n, 0);

        return result;
    }

    Digit carry = 0;
    for (unsigned i = 0; i < n; i++) {
        Digit d = x.digit(i);
        result->setDigit(i, (d << shift) | carry);
        carry = d >> (digitBits - shift);
    }

    if (mode == LeftShiftMode::AlwaysAddOneDigit)
        result->setDigit(n, carry);
    else {
        ASSERT(mode == LeftShiftMode::SameSizeResult);
        ASSERT(!carry);
    }

    return result;
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
template <typename BigIntImpl1, typename BigIntImpl2, typename BitwiseOp>
inline JSBigInt* JSBigInt::absoluteBitwiseOp(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y, ExtraDigitsHandling extraDigits, BitwiseOp&& op)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned xLength = x.length();
    unsigned yLength = y.length();
    unsigned numPairs = yLength;
    unsigned maxLength = xLength;
    if (xLength < yLength) {
        numPairs = xLength;
        maxLength = yLength;
    }

    ASSERT(numPairs == std::min(xLength, yLength));
    ASSERT(maxLength == std::max(xLength, yLength));
    unsigned resultLength = extraDigits == ExtraDigitsHandling::Copy ? maxLength : numPairs;
    JSBigInt* result = createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, nullptr);
    unsigned i = 0;
    for (; i < numPairs; i++)
        result->setDigit(i, op(x.digit(i), y.digit(i)));

    if (extraDigits == ExtraDigitsHandling::Copy) {
        if (xLength > yLength) {
            for (; i < xLength; i++)
                result->setDigit(i, x.digit(i));
        } else {
            for (; i < yLength; i++)
                result->setDigit(i, y.digit(i));
        }
    }

    for (; i < resultLength; i++)
        result->setDigit(i, 0);

    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt* JSBigInt::absoluteAnd(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    auto digitOperation = [](Digit a, Digit b) {
        return a & b;
    };
    return absoluteBitwiseOp(globalObject, x, y, ExtraDigitsHandling::Skip, digitOperation);
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt* JSBigInt::absoluteOr(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    auto digitOperation = [](Digit a, Digit b) {
        return a | b;
    };
    return absoluteBitwiseOp(globalObject, x, y, ExtraDigitsHandling::Copy, digitOperation);
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt* JSBigInt::absoluteAndNot(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    // x & ~y

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned xLength = x.length();
    unsigned yLength = y.length();
    unsigned resultLength = xLength;

    JSBigInt* result = createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, nullptr);
    unsigned i = 0;
    for (; i < std::min(xLength, yLength); i++)
        result->setDigit(i, x.digit(i) & ~y.digit(i));
    for (; i < resultLength; ++i)
        result->setDigit(i, x.digit(i));

    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt* JSBigInt::absoluteXor(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    auto digitOperation = [](Digit a, Digit b) {
        return a ^ b;
    };
    return absoluteBitwiseOp(globalObject, x, y, ExtraDigitsHandling::Copy, digitOperation);
}
    
template <typename BigIntImpl>
JSBigInt* JSBigInt::absoluteAddOne(JSGlobalObject* globalObject, BigIntImpl x, SignOption signOption)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned inputLength = x.length();
    // The addition will overflow into a new digit if all existing digits are
    // at maximum.
    bool willOverflow = true;
    for (unsigned i = 0; i < inputLength; i++) {
        if (std::numeric_limits<Digit>::max() != x.digit(i)) {
            willOverflow = false;
            break;
        }
    }

    unsigned resultLength = inputLength + willOverflow;
    JSBigInt* result = createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Digit carry = 1;
    for (unsigned i = 0; i < inputLength; i++) {
        Digit newCarry = 0;
        result->setDigit(i, digitAdd(x.digit(i), carry, newCarry));
        carry = newCarry;
    }
    if (resultLength > inputLength)
        result->setDigit(inputLength, carry);
    else
        ASSERT(!carry);

    result->setSign(signOption == SignOption::Signed);
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

template <typename BigIntImpl>
JSBigInt* JSBigInt::absoluteSubOne(JSGlobalObject* globalObject, BigIntImpl x, unsigned resultLength)
{
    ASSERT(!x.isZero());
    ASSERT(resultLength >= x.length());
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSBigInt* result = createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned length = x.length();
    Digit borrow = 1;
    for (unsigned i = 0; i < length; i++) {
        Digit newBorrow = 0;
        result->setDigit(i, digitSub(x.digit(i), borrow, newBorrow));
        borrow = newBorrow;
    }
    ASSERT(!borrow);
    for (unsigned i = length; i < resultLength; i++)
        result->setDigit(i, borrow);

    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
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
    unsigned digitShift = static_cast<unsigned>(shift / digitBits);
    unsigned bitsShift = static_cast<unsigned>(shift % digitBits);
    unsigned length = x.length();
    bool grow = bitsShift && (x.digit(length - 1) >> (digitBits - bitsShift));
    int resultLength = length + digitShift + grow;
    if (static_cast<unsigned>(resultLength) > maxLength) {
        throwOutOfMemoryError(globalObject, scope, "BigInt generated from this operation is too big"_s);
        return nullptr;
    }

    JSBigInt* result = createWithLength(globalObject, resultLength);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!bitsShift) {
        unsigned i = 0;
        for (; i < digitShift; i++)
            result->setDigit(i, 0ul);

        for (; i < static_cast<unsigned>(resultLength); i++)
            result->setDigit(i, x.digit(i - digitShift));
    } else {
        Digit carry = 0;
        for (unsigned i = 0; i < digitShift; i++)
            result->setDigit(i, 0ul);

        for (unsigned i = 0; i < length; i++) {
            Digit d = x.digit(i);
            result->setDigit(i + digitShift, (d << bitsShift) | carry);
            carry = d >> (digitBits - bitsShift);
        }

        if (grow)
            result->setDigit(length + digitShift, carry);
        else
            ASSERT(!carry);
    }

    result->setSign(x.sign());
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

template <typename BigIntImpl1, typename BigIntImpl2>
JSBigInt::ImplResult JSBigInt::rightShiftByAbsolute(JSGlobalObject* globalObject, BigIntImpl1 x, BigIntImpl2 y)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = x.length();
    bool sign = x.sign();
    auto optionalShift = toShiftAmount(y);
    if (!optionalShift)
        RELEASE_AND_RETURN(scope, rightShiftByMaximum(globalObject, sign));

    Digit shift = *optionalShift;
    unsigned digitalShift = static_cast<unsigned>(shift / digitBits);
    unsigned bitsShift = static_cast<unsigned>(shift % digitBits);
    int resultLength = length - digitalShift;
    if (resultLength <= 0)
        RELEASE_AND_RETURN(scope, rightShiftByMaximum(globalObject, sign));

    // For negative numbers, round down if any bit was shifted out (so that e.g.
    // -5n >> 1n == -3n and not -2n). Check now whether this will happen and
    // whether it can cause overflow into a new digit. If we allocate the result
    // large enough up front, it avoids having to do a second allocation later.
    bool mustRoundDown = false;
    if (sign) {
        const Digit mask = (static_cast<Digit>(1) << bitsShift) - 1;
        if (x.digit(digitalShift) & mask)
            mustRoundDown = true;
        else {
            for (unsigned i = 0; i < digitalShift; i++) {
                if (x.digit(i)) {
                    mustRoundDown = true;
                    break;
                }
            }
        }
    }

    // If bitsShift is non-zero, it frees up bits, preventing overflow.
    if (mustRoundDown && !bitsShift) {
        // Overflow cannot happen if the most significant digit has unset bits.
        Digit msd = x.digit(length - 1);
        bool roundingCanOverflow = !static_cast<Digit>(~msd);
        if (roundingCanOverflow)
            resultLength++;
    }

    ASSERT(static_cast<unsigned>(resultLength) <= length);
    JSBigInt* result = createWithLength(globalObject, static_cast<unsigned>(resultLength));
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (!bitsShift) {
        result->setDigit(resultLength - 1, 0);
        for (unsigned i = digitalShift; i < length; i++)
            result->setDigit(i - digitalShift, x.digit(i));
    } else {
        Digit carry = x.digit(digitalShift) >> bitsShift;
        unsigned last = length - digitalShift - 1;
        for (unsigned i = 0; i < last; i++) {
            Digit d = x.digit(i + digitalShift + 1);
            result->setDigit(i, (d << (digitBits - bitsShift)) | carry);
            carry = d >> bitsShift;
        }
        result->setDigit(last, carry);
    }

    if (sign) {
        result->setSign(true);
        if (mustRoundDown) {
            // Since the result is negative, rounding down means adding one to
            // its absolute value. This cannot overflow.
            result = result->rightTrim(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            RELEASE_AND_RETURN(scope, absoluteAddOne(globalObject, HeapBigIntImpl { result }, SignOption::Signed));
        }
    }

    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
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

    Vector<LChar> resultString(charsRequired);
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
    return StringImpl::adopt(WTFMove(resultString));
}

String JSBigInt::toStringGeneric(VM& vm, JSGlobalObject* nullOrGlobalObjectForOOM, JSBigInt* x, unsigned radix)
{
    // FIXME: [JSC] Revisit usage of Vector into JSBigInt::toString
    // https://bugs.webkit.org/show_bug.cgi?id=180671
    Vector<LChar> resultString;

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
        unsigned nonZeroDigit = length - 1;
        ASSERT(x->digit(nonZeroDigit));

        // {rest} holds the part of the BigInt that we haven't looked at yet.
        // Not to be confused with "remainder"!
        JSBigInt* rest = nullptr;

        // In the first round, divide the input, allocating a new BigInt for
        // the result == rest; from then on divide the rest in-place.
        JSBigInt** dividend = &x;
        do {
            Digit chunk;
            bool success = absoluteDivWithDigitDivisor(nullOrGlobalObjectForOOM, vm, HeapBigIntImpl { *dividend }, chunkDivisor, &rest, chunk);
            if (!success)
                return String();
            dividend = &rest;
            for (unsigned i = 0; i < chunkChars; i++) {
                resultString.append(radixDigits[chunk % radix]);
                chunk /= radix;
            }
            ASSERT(!chunk);

            if (!rest->digit(nonZeroDigit))
                nonZeroDigit--;

            // We can never clear more than one digit per iteration, because
            // chunkDivisor is smaller than max digit value.
            ASSERT(rest->digit(nonZeroDigit));
        } while (nonZeroDigit > 0);

        lastDigit = rest->digit(0);
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

    std::reverse(resultString.begin(), resultString.end());

    return StringImpl::adopt(WTFMove(resultString));
}

JSBigInt* JSBigInt::rightTrim(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm)
{
    if (isZero()) {
        ASSERT(!sign());
        return this;
    }

    int nonZeroIndex = m_length - 1;
    while (nonZeroIndex >= 0 && !digit(nonZeroIndex))
        nonZeroIndex--;

    if (nonZeroIndex < 0)
        return createZero(nullOrGlobalObjectForOOM, vm);

    if (nonZeroIndex == static_cast<int>(m_length - 1))
        return this;

    unsigned newLength = nonZeroIndex + 1;
    JSBigInt* trimmedBigInt = createWithLength(nullOrGlobalObjectForOOM, vm, newLength);
    if (UNLIKELY(!trimmedBigInt))
        return nullptr;
    std::copy(dataStorage(), dataStorage() + newLength, trimmedBigInt->dataStorage());

    trimmedBigInt->setSign(this->sign());

    ensureStillAliveHere(this);

    return trimmedBigInt;
}

JSBigInt* JSBigInt::rightTrim(JSGlobalObject* globalObject)
{
    return rightTrim(globalObject, globalObject->vm());
}

JSBigInt* JSBigInt::tryRightTrim(VM& vm)
{
    return rightTrim(nullptr, vm);
}

JSBigInt* JSBigInt::allocateFor(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, unsigned radix, unsigned charcount)
{
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
                return createWithLength(nullOrGlobalObjectForOOM, vm, length);
        }
    }

    if (nullOrGlobalObjectForOOM) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope, "BigInt generated from this operation is too big"_s);
    }

    return nullptr;
}

size_t JSBigInt::estimatedSize(JSCell* cell, VM& vm)
{
    return Base::estimatedSize(cell, vm) + jsCast<JSBigInt*>(cell)->m_length * sizeof(Digit);
}

double JSBigInt::toNumber(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(globalObject, scope, "Conversion from 'BigInt' to 'number' is not allowed."_s);
    return 0.0;
}

template <typename CharType>
JSValue JSBigInt::parseInt(JSGlobalObject* globalObject, CharType*  data, unsigned length, ErrorParseMode errorParseMode)
{
    VM& vm = globalObject->vm();

    unsigned p = 0;
    while (p < length && isStrWhiteSpace(data[p]))
        ++p;

    // Check Radix from first characters
    if (static_cast<unsigned>(p) + 1 < static_cast<unsigned>(length) && data[p] == '0') {
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'b'))
            return parseInt(globalObject, vm, data, length, p + 2, 2, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'x'))
            return parseInt(globalObject, vm, data, length, p + 2, 16, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'o'))
            return parseInt(globalObject, vm, data, length, p + 2, 8, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
    }

    ParseIntSign sign = ParseIntSign::Unsigned;
    if (p < length) {
        if (data[p] == '-') {
            sign = ParseIntSign::Signed;
            ++p;
        } else if (data[p] == '+')
            ++p;
    }

    return parseInt(globalObject, vm, data, length, p, 10, errorParseMode, sign);
}

template <typename CharType>
JSValue JSBigInt::parseInt(JSGlobalObject* nullOrGlobalObjectForOOM, VM& vm, CharType* data, unsigned length, unsigned startIndex, unsigned radix, ErrorParseMode errorParseMode, ParseIntSign sign, ParseIntMode parseMode)
{
    unsigned p = startIndex;

    if (parseMode != ParseIntMode::AllowEmptyString && startIndex == length) {
        ASSERT(nullOrGlobalObjectForOOM);
        if (errorParseMode == ErrorParseMode::ThrowExceptions) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwVMError(nullOrGlobalObjectForOOM, scope, createSyntaxError(nullOrGlobalObjectForOOM, "Failed to parse String to BigInt"_s));
        }
        return JSValue();
    }

    // Skipping leading zeros
    while (p < length && data[p] == '0')
        ++p;

    int endIndex = length - 1;
    // Removing trailing spaces
    while (endIndex >= static_cast<int>(p) && isStrWhiteSpace(data[endIndex]))
        --endIndex;

    length = endIndex + 1;

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

    JSBigInt* heapResult = nullptr;

    unsigned limit0 = '0' + (radix < 10 ? radix : 10);
    unsigned limita = 'a' + (static_cast<int32_t>(radix) - 10);
    unsigned limitA = 'A' + (static_cast<int32_t>(radix) - 10);
    unsigned initialLength = length - p;
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

        if (!heapResult) {
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
            heapResult = allocateFor(nullOrGlobalObjectForOOM, vm, radix, initialLength);
            if (UNLIKELY(!heapResult))
                return JSValue();
            heapResult->initialize(InitializationType::WithZero);
        }

        ASSERT(static_cast<uint64_t>(static_cast<Digit>(multiplier)) == multiplier);
        ASSERT(static_cast<uint64_t>(static_cast<Digit>(digit)) == digit);
        heapResult->inplaceMultiplyAdd(static_cast<Digit>(multiplier), static_cast<Digit>(digit));
    }

    heapResult->setSign(sign == ParseIntSign::Signed);
    return heapResult->rightTrim(nullOrGlobalObjectForOOM, vm);
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
    // This algorithm expect that the double format is IEEE 754

    uint64_t doubleBits = bitwise_cast<uint64_t>(y);
    int rawExponent = static_cast<int>(doubleBits >> 52) & 0x7FF;

    if (rawExponent == 0x7FF) {
        if (std::isnan(y))
            return ComparisonResult::Undefined;

        return (y == std::numeric_limits<double>::infinity()) ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    }

    bool xSign = x->sign();
    
    // Note that this is different from the double's sign bit for -0. That's
    // intentional because -0 must be treated like 0.
    bool ySign = y < 0;
    if (xSign != ySign)
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    if (!y) {
        ASSERT(!xSign);
        return x->isZero() ? ComparisonResult::Equal : ComparisonResult::GreaterThan;
    }

    if (x->isZero())
        return ComparisonResult::LessThan;

    uint64_t mantissa = doubleBits & 0x000FFFFFFFFFFFFF;

    // Non-finite doubles are handled above.
    ASSERT(rawExponent != 0x7FF);
    int exponent = rawExponent - 0x3FF;
    if (exponent < 0) {
        // The absolute value of the double is less than 1. Only 0n has an
        // absolute value smaller than that, but we've already covered that case.
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    }

    int xLength = x->length();
    Digit xMSD = x->digit(xLength - 1);
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

        Digit digit = x->digit(digitIndex);
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
    return jsNumber(bitwise_cast<double>(doubleBits));
}

template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::asIntNImpl(JSGlobalObject* globalObject, uint64_t n, BigIntImpl bigInt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (bigInt.isZero())
        return bigInt;
    if (n == 0)
        RELEASE_AND_RETURN(scope, zeroImpl(globalObject));

    uint64_t neededLength = (n + digitBits - 1) / digitBits;
    uint64_t length = static_cast<uint64_t>(bigInt.length());
    // If bigInt has less than n bits, return it directly.
    if (length < neededLength)
        return bigInt;
    ASSERT(neededLength <= INT32_MAX);
    Digit topDigit = bigInt.digit(static_cast<int32_t>(neededLength) - 1);
    Digit compareDigit = static_cast<Digit>(1) << ((n - 1) % digitBits);
    if (length == neededLength && topDigit < compareDigit)
        return bigInt;

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
            return bigInt;
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
        return bigInt;
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
        return bigInt;
    static_assert(maxLengthBits < INT32_MAX - digitBits);
    int32_t neededLength = static_cast<int32_t>((n + digitBits - 1) / digitBits);
    if (static_cast<int32_t>(bigInt.length()) < neededLength)
        return bigInt;

    int32_t bitsInTopDigit = n % digitBits;
    if (static_cast<int32_t>(bigInt.length()) == neededLength) {
        if (bitsInTopDigit == 0)
            return bigInt;
        Digit topDigit = bigInt.digit(neededLength - 1);
        if ((topDigit >> bitsInTopDigit) == 0)
            return bigInt;
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

    ASSERT(n != 0);
    ASSERT(bigInt.length() > n / digitBits);

    int32_t neededDigits = (n + (digitBits - 1)) / digitBits;
    ASSERT(neededDigits <= static_cast<int32_t>(bigInt.length()));
    JSBigInt* result = createWithLength(globalObject, neededDigits);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result);

    // Copy all digits except the MSD.
    int32_t last = neededDigits - 1;
    for (int32_t i = 0; i < last; i++)
        result->setDigit(i, bigInt.digit(i));

    // The MSD might contain extra bits that we don't want.
    Digit msd = bigInt.digit(last);
    if (n % digitBits != 0) {
        int32_t drop = digitBits - (n % digitBits);
        msd = (msd << drop) >> drop;
    }
    result->setDigit(last, msd);
    result->setSign(bigInt.sign());
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
}

// Subtracts the least significant n bits of abs(bigInt) from 2^n.
template <typename BigIntImpl>
JSBigInt::ImplResult JSBigInt::truncateAndSubFromPowerOfTwo(JSGlobalObject* globalObject, int32_t n, BigIntImpl bigInt, bool resultSign)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(n != 0);
    ASSERT(n <= static_cast<int32_t>(maxLengthBits));

    int32_t neededDigits = (n + (digitBits - 1)) / digitBits;
    ASSERT(neededDigits <= static_cast<int32_t>(maxLength)); // Follows from n <= maxLengthBits.
    JSBigInt* result = createWithLength(globalObject, neededDigits);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result);

    // Process all digits except the MSD.
    int32_t i = 0;
    int32_t last = neededDigits - 1;
    int32_t length = bigInt.length();
    Digit borrow = 0;
    // Take digits from bigInt unless its length is exhausted.
    int32_t limit = std::min(last, length);
    for (; i < limit; i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(0, bigInt.digit(i), newBorrow);
        difference = digitSub(difference, borrow, newBorrow);
        result->setDigit(i, difference);
        borrow = newBorrow;
    }
    // Then simulate leading zeroes in {bigInt} as needed.
    for (; i < last; i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(0, borrow, newBorrow);
        result->setDigit(i, difference);
        borrow = newBorrow;
    }

    // The MSD might contain extra bits that we don't want.
    Digit msd = last < length ? bigInt.digit(last) : 0;
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
    result->setDigit(last, resultMSD);
    result->setSign(resultSign);
    RELEASE_AND_RETURN(scope, result->rightTrim(globalObject));
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
    m_hash = computeHash(dataStorage(), length(), m_sign);
    return m_hash;
}

} // namespace JSC
