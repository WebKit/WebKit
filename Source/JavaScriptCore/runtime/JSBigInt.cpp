/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "CatchScope.h"
#include "JSCInlines.h"
#include "MathCommon.h"
#include "ParseInt.h"
#include <algorithm>
#include <wtf/MathExtras.h>

#define STATIC_ASSERT(cond) static_assert(cond, "JSBigInt assumes " #cond)

namespace JSC {

const ClassInfo JSBigInt::s_info =
    { "JSBigInt", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSBigInt) };

JSBigInt::JSBigInt(VM& vm, Structure* structure, unsigned length)
    : Base(vm, structure)
    , m_length(length)
    , m_sign(false)
{ }

void JSBigInt::initialize(InitializationType initType)
{
    if (initType == InitializationType::WithZero)
        memset(dataStorage(), 0, length() * sizeof(Digit));
}

Structure* JSBigInt::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(BigIntType, StructureFlags), info());
}

JSBigInt* JSBigInt::createZero(VM& vm)
{
    JSBigInt* zeroBigInt = createWithLength(vm, 0);
    return zeroBigInt;
}

inline size_t JSBigInt::allocationSize(unsigned length)
{
    size_t sizeWithPadding = WTF::roundUpToMultipleOf<sizeof(size_t)>(sizeof(JSBigInt));
    return sizeWithPadding + length * sizeof(Digit);
}

JSBigInt* JSBigInt::createWithLength(VM& vm, unsigned length)
{
    JSBigInt* bigInt = new (NotNull, allocateCell<JSBigInt>(vm.heap, allocationSize(length))) JSBigInt(vm, vm.bigIntStructure.get(), length);
    bigInt->finishCreation(vm);
    return bigInt;
}

JSBigInt* JSBigInt::createFrom(VM& vm, int32_t value)
{
    if (!value)
        return createZero(vm);
    
    JSBigInt* bigInt = createWithLength(vm, 1);
    
    if (value < 0) {
        bigInt->setDigit(0, static_cast<Digit>(-1 * static_cast<int64_t>(value)));
        bigInt->setSign(true);
    } else
        bigInt->setDigit(0, static_cast<Digit>(value));

    return bigInt;
}

JSBigInt* JSBigInt::createFrom(VM& vm, uint32_t value)
{
    if (!value)
        return createZero(vm);
    
    JSBigInt* bigInt = createWithLength(vm, 1);
    bigInt->setDigit(0, static_cast<Digit>(value));
    return bigInt;
}

JSBigInt* JSBigInt::createFrom(VM& vm, int64_t value)
{
    if (!value)
        return createZero(vm);
    
    if (sizeof(Digit) == 8) {
        JSBigInt* bigInt = createWithLength(vm, 1);
        
        if (value < 0) {
            bigInt->setDigit(0, static_cast<Digit>(static_cast<uint64_t>(-(value + 1)) + 1));
            bigInt->setSign(true);
        } else
            bigInt->setDigit(0, static_cast<Digit>(value));
        
        return bigInt;
    }
    
    JSBigInt* bigInt = createWithLength(vm, 2);
    
    uint64_t tempValue;
    bool sign = false;
    if (value < 0) {
        tempValue = static_cast<uint64_t>(-(value + 1)) + 1;
        sign = true;
    } else
        tempValue = value;
    
    Digit lowBits  = static_cast<Digit>(tempValue & 0xffffffff);
    Digit highBits = static_cast<Digit>((tempValue >> 32) & 0xffffffff);
    
    bigInt->setDigit(0, lowBits);
    bigInt->setDigit(1, highBits);
    bigInt->setSign(sign);
    
    return bigInt;
}

JSBigInt* JSBigInt::createFrom(VM& vm, bool value)
{
    if (!value)
        return createZero(vm);
    
    JSBigInt* bigInt = createWithLength(vm, 1);
    bigInt->setDigit(0, static_cast<Digit>(value));
    return bigInt;
}

JSValue JSBigInt::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    return const_cast<JSBigInt*>(this);
}

std::optional<uint8_t> JSBigInt::singleDigitValueForString()
{
    if (isZero())
        return 0;
    
    if (length() == 1 && !sign()) {
        Digit rDigit = digit(0);
        if (rDigit <= 9)
            return static_cast<uint8_t>(rDigit);
    }
    return { };
}

JSBigInt* JSBigInt::parseInt(ExecState* exec, StringView s, ErrorParseMode parserMode)
{
    if (s.is8Bit())
        return parseInt(exec, s.characters8(), s.length(), parserMode);
    return parseInt(exec, s.characters16(), s.length(), parserMode);
}

JSBigInt* JSBigInt::parseInt(ExecState* exec, VM& vm, StringView s, uint8_t radix, ErrorParseMode parserMode, ParseIntSign sign)
{
    if (s.is8Bit())
        return parseInt(exec, vm, s.characters8(), s.length(), 0, radix, parserMode, sign, ParseIntMode::DisallowEmptyString);
    return parseInt(exec, vm, s.characters16(), s.length(), 0, radix, parserMode, sign, ParseIntMode::DisallowEmptyString);
}

JSBigInt* JSBigInt::stringToBigInt(ExecState* exec, StringView s)
{
    return parseInt(exec, s, ErrorParseMode::IgnoreExceptions);
}

String JSBigInt::toString(ExecState* exec, unsigned radix)
{
    if (this->isZero())
        return exec->vm().smallStrings.singleCharacterStringRep('0');

    if (hasOneBitSet(radix))
        return toStringBasePowerOfTwo(exec, this, radix);

    return toStringGeneric(exec, this, radix);
}

inline bool JSBigInt::isZero()
{
    ASSERT(length() || !sign());
    return length() == 0;
}

// Multiplies {this} with {factor} and adds {summand} to the result.
inline void JSBigInt::inplaceMultiplyAdd(uintptr_t factor, uintptr_t summand)
{
    STATIC_ASSERT(sizeof(factor) == sizeof(Digit));
    STATIC_ASSERT(sizeof(summand) == sizeof(Digit));

    internalMultiplyAdd(this, factor, summand, length(), this);
}

JSBigInt* JSBigInt::multiply(ExecState* exec, JSBigInt* x, JSBigInt* y)
{
    VM& vm = exec->vm();

    if (x->isZero())
        return x;
    if (y->isZero())
        return y;

    unsigned resultLength = x->length() + y->length();
    JSBigInt* result = JSBigInt::createWithLength(vm, resultLength);
    result->initialize(InitializationType::WithZero);

    for (unsigned i = 0; i < x->length(); i++)
        multiplyAccumulate(y, x->digit(i), result, i);

    result->setSign(x->sign() != y->sign());
    return result->rightTrim(vm);
}

JSBigInt* JSBigInt::divide(ExecState* exec, JSBigInt* x, JSBigInt* y)
{
    // 1. If y is 0n, throw a RangeError exception.
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (y->isZero()) {
        throwRangeError(exec, scope, "0 is an invalid divisor value."_s);
        return nullptr;
    }

    // 2. Let quotient be the mathematical value of x divided by y.
    // 3. Return a BigInt representing quotient rounded towards 0 to the next
    //    integral value.
    if (absoluteCompare(x, y) == ComparisonResult::LessThan)
        return createZero(vm);

    JSBigInt* quotient = nullptr;
    bool resultSign = x->sign() != y->sign();
    if (y->length() == 1) {
        Digit divisor = y->digit(0);
        if (divisor == 1)
            return resultSign == x->sign() ? x : unaryMinus(vm, x);

        Digit remainder;
        absoluteDivWithDigitDivisor(vm, x, divisor, &quotient, remainder);
    } else
        absoluteDivWithBigIntDivisor(vm, x, y, &quotient, nullptr);

    quotient->setSign(resultSign);
    return quotient->rightTrim(vm);
}

JSBigInt* JSBigInt::copy(VM& vm, JSBigInt* x)
{
    ASSERT(!x->isZero());

    JSBigInt* result = JSBigInt::createWithLength(vm, x->length());
    std::copy(x->dataStorage(), x->dataStorage() + x->length(), result->dataStorage());
    result->setSign(x->sign());
    return result;
}

JSBigInt* JSBigInt::unaryMinus(VM& vm, JSBigInt* x)
{
    if (x->isZero())
        return x;

    JSBigInt* result = copy(vm, x);
    result->setSign(!x->sign());
    return result;
}

JSBigInt* JSBigInt::remainder(ExecState* exec, JSBigInt* x, JSBigInt* y)
{
    // 1. If y is 0n, throw a RangeError exception.
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (y->isZero()) {
        throwRangeError(exec, scope, "0 is an invalid divisor value."_s);
        return nullptr;
    }

    // 2. Return the JSBigInt representing x modulo y.
    // See https://github.com/tc39/proposal-bigint/issues/84 though.
    if (absoluteCompare(x, y) == ComparisonResult::LessThan)
        return x;

    JSBigInt* remainder;
    if (y->length() == 1) {
        Digit divisor = y->digit(0);
        if (divisor == 1)
            return createZero(vm);

        Digit remainderDigit;
        absoluteDivWithDigitDivisor(vm, x, divisor, nullptr, remainderDigit);
        if (!remainderDigit)
            return createZero(vm);

        remainder = createWithLength(vm, 1);
        remainder->setDigit(0, remainderDigit);
    } else
        absoluteDivWithBigIntDivisor(vm, x, y, nullptr, &remainder);

    remainder->setSign(x->sign());
    return remainder->rightTrim(vm);
}

JSBigInt* JSBigInt::add(VM& vm, JSBigInt* x, JSBigInt* y)
{
    bool xSign = x->sign();

    // x + y == x + y
    // -x + -y == -(x + y)
    if (xSign == y->sign())
        return absoluteAdd(vm, x, y, xSign);

    // x + -y == x - y == -(y - x)
    // -x + y == y - x == -(x - y)
    ComparisonResult comparisonResult = absoluteCompare(x, y);
    if (comparisonResult == ComparisonResult::GreaterThan || comparisonResult == ComparisonResult::Equal)
        return absoluteSub(vm, x, y, xSign);

    return absoluteSub(vm, y, x, !xSign);
}

JSBigInt* JSBigInt::sub(VM& vm, JSBigInt* x, JSBigInt* y)
{
    bool xSign = x->sign();
    if (xSign != y->sign()) {
        // x - (-y) == x + y
        // (-x) - y == -(x + y)
        return absoluteAdd(vm, x, y, xSign);
    }
    // x - y == -(y - x)
    // (-x) - (-y) == y - x == -(x - y)
    ComparisonResult comparisonResult = absoluteCompare(x, y);
    if (comparisonResult == ComparisonResult::GreaterThan || comparisonResult == ComparisonResult::Equal)
        return absoluteSub(vm, x, y, xSign);

    return absoluteSub(vm, y, x, !xSign);
}

JSBigInt* JSBigInt::bitwiseAnd(VM& vm, JSBigInt* x, JSBigInt* y)
{
    if (!x->sign() && !y->sign())
        return absoluteAnd(vm, x, y);
    
    if (x->sign() && y->sign()) {
        int resultLength = std::max(x->length(), y->length()) + 1;
        // (-x) & (-y) == ~(x-1) & ~(y-1) == ~((x-1) | (y-1))
        // == -(((x-1) | (y-1)) + 1)
        JSBigInt* result = absoluteSubOne(vm, x, resultLength);
        JSBigInt* y1 = absoluteSubOne(vm, y, y->length());
        result = absoluteOr(vm, result, y1);

        return absoluteAddOne(vm, result, SignOption::Signed);
    }

    ASSERT(x->sign() != y->sign());
    // Assume that x is the positive BigInt.
    if (x->sign())
        std::swap(x, y);
    
    // x & (-y) == x & ~(y-1) == x & ~(y-1)
    return absoluteAndNot(vm, x, absoluteSubOne(vm, y, y->length()));
}

JSBigInt* JSBigInt::bitwiseOr(VM& vm, JSBigInt* x, JSBigInt* y)
{
    unsigned resultLength = std::max(x->length(), y->length());

    if (!x->sign() && !y->sign())
        return absoluteOr(vm, x, y);
    
    if (x->sign() && y->sign()) {
        // (-x) | (-y) == ~(x-1) | ~(y-1) == ~((x-1) & (y-1))
        // == -(((x-1) & (y-1)) + 1)
        JSBigInt* result = absoluteSubOne(vm, x, resultLength);
        JSBigInt* y1 = absoluteSubOne(vm, y, y->length());
        result = absoluteAnd(vm, result, y1);
        return absoluteAddOne(vm, result, SignOption::Signed);
    }
    
    ASSERT(x->sign() != y->sign());

    // Assume that x is the positive BigInt.
    if (x->sign())
        std::swap(x, y);
    
    // x | (-y) == x | ~(y-1) == ~((y-1) &~ x) == -(((y-1) &~ x) + 1)
    JSBigInt* result = absoluteSubOne(vm, y, resultLength);
    result = absoluteAndNot(vm, result, x);
    return absoluteAddOne(vm, result, SignOption::Signed);
}

#if USE(JSVALUE32_64)
#define HAVE_TWO_DIGIT 1
typedef uint64_t TwoDigit;
#elif HAVE(INT128_T)
#define HAVE_TWO_DIGIT 1
typedef __uint128_t TwoDigit;
#else
#define HAVE_TWO_DIGIT 0
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
#if HAVE(TWO_DIGIT)
    TwoDigit result = static_cast<TwoDigit>(a) * static_cast<TwoDigit>(b);
    high = result >> digitBits;

    return static_cast<Digit>(result);
#else
    // Multiply in half-pointer-sized chunks.
    // For inputs [AH AL]*[BH BL], the result is:
    //
    //            [AL*BL]  // rLow
    //    +    [AL*BH]     // rMid1
    //    +    [AH*BL]     // rMid2
    //    + [AH*BH]        // rHigh
    //    = [R4 R3 R2 R1]  // high = [R4 R3], low = [R2 R1]
    //
    // Where of course we must be careful with carries between the columns.
    Digit aLow = a & halfDigitMask;
    Digit aHigh = a >> halfDigitBits;
    Digit bLow = b & halfDigitMask;
    Digit bHigh = b >> halfDigitBits;
    
    Digit rLow = aLow * bLow;
    Digit rMid1 = aLow * bHigh;
    Digit rMid2 = aHigh * bLow;
    Digit rHigh = aHigh * bHigh;
    
    Digit carry = 0;
    Digit low = digitAdd(rLow, rMid1 << halfDigitBits, carry);
    low = digitAdd(low, rMid2 << halfDigitBits, carry);

    high = (rMid1 >> halfDigitBits) + (rMid2 >> halfDigitBits) + rHigh + carry;

    return low;
#endif
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
#if USE(JSVALUE64)
    unsigned s = clz64(divisor);
#else
    unsigned s = clz32(divisor);
#endif
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
    STATIC_ASSERT(sizeof(intptr_t) == sizeof(Digit));
    Digit sZeroMask = static_cast<Digit>((-static_cast<intptr_t>(s)) >> (digitBits - 1));
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
void JSBigInt::internalMultiplyAdd(JSBigInt* source, Digit factor, Digit summand, unsigned n, JSBigInt* result)
{
    ASSERT(source->length() >= n);
    ASSERT(result->length() >= n);

    Digit carry = summand;
    Digit high = 0;
    for (unsigned i = 0; i < n; i++) {
        Digit current = source->digit(i);
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
void JSBigInt::multiplyAccumulate(JSBigInt* multiplicand, Digit multiplier, JSBigInt* accumulator, unsigned accumulatorIndex)
{
    ASSERT(accumulator->length() > multiplicand->length() + accumulatorIndex);
    if (!multiplier)
        return;
    
    Digit carry = 0;
    Digit high = 0;
    for (unsigned i = 0; i < multiplicand->length(); i++, accumulatorIndex++) {
        Digit acc = accumulator->digit(accumulatorIndex);
        Digit newCarry = 0;
        
        // Add last round's carryovers.
        acc = digitAdd(acc, high, newCarry);
        acc = digitAdd(acc, carry, newCarry);
        
        // Compute this round's multiplication.
        Digit multiplicandDigit = multiplicand->digit(i);
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

JSBigInt::ComparisonResult JSBigInt::compare(JSBigInt* x, JSBigInt* y)
{
    bool xSign = x->sign();

    if (xSign != y->sign())
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    ComparisonResult result = absoluteCompare(x, y);
    if (result == ComparisonResult::GreaterThan)
        return xSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
    if (result == ComparisonResult::LessThan)
        return xSign ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;

    return ComparisonResult::Equal; 
}

inline JSBigInt::ComparisonResult JSBigInt::absoluteCompare(JSBigInt* x, JSBigInt* y)
{
    ASSERT(!x->length() || x->digit(x->length() - 1));
    ASSERT(!y->length() || y->digit(y->length() - 1));

    int diff = x->length() - y->length();
    if (diff)
        return diff < 0 ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;

    int i = x->length() - 1;
    while (i >= 0 && x->digit(i) == y->digit(i))
        i--;

    if (i < 0)
        return ComparisonResult::Equal;

    return x->digit(i) > y->digit(i) ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
}

JSBigInt* JSBigInt::absoluteAdd(VM& vm, JSBigInt* x, JSBigInt* y, bool resultSign)
{
    if (x->length() < y->length())
        return absoluteAdd(vm, y, x, resultSign);

    if (x->isZero()) {
        ASSERT(y->isZero());
        return x;
    }

    if (y->isZero())
        return resultSign == x->sign() ? x : unaryMinus(vm, x);

    JSBigInt* result = JSBigInt::createWithLength(vm, x->length() + 1);
    ASSERT(result);
    Digit carry = 0;
    unsigned i = 0;
    for (; i < y->length(); i++) {
        Digit newCarry = 0;
        Digit sum = digitAdd(x->digit(i), y->digit(i), newCarry);
        sum = digitAdd(sum, carry, newCarry);
        result->setDigit(i, sum);
        carry = newCarry;
    }

    for (; i < x->length(); i++) {
        Digit newCarry = 0;
        Digit sum = digitAdd(x->digit(i), carry, newCarry);
        result->setDigit(i, sum);
        carry = newCarry;
    }

    result->setDigit(i, carry);
    result->setSign(resultSign);

    return result->rightTrim(vm);
}

JSBigInt* JSBigInt::absoluteSub(VM& vm, JSBigInt* x, JSBigInt* y, bool resultSign)
{
    ComparisonResult comparisonResult = absoluteCompare(x, y);
    ASSERT(x->length() >= y->length());
    ASSERT(comparisonResult == ComparisonResult::GreaterThan || comparisonResult == ComparisonResult::Equal);

    if (x->isZero()) {
        ASSERT(y->isZero());
        return x;
    }

    if (y->isZero())
        return resultSign == x->sign() ? x : unaryMinus(vm, x);

    if (comparisonResult == ComparisonResult::Equal)
        return JSBigInt::createZero(vm);

    JSBigInt* result = JSBigInt::createWithLength(vm, x->length());
    Digit borrow = 0;
    unsigned i = 0;
    for (; i < y->length(); i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(x->digit(i), y->digit(i), newBorrow);
        difference = digitSub(difference, borrow, newBorrow);
        result->setDigit(i, difference);
        borrow = newBorrow;
    }

    for (; i < x->length(); i++) {
        Digit newBorrow = 0;
        Digit difference = digitSub(x->digit(i), borrow, newBorrow);
        result->setDigit(i, difference);
        borrow = newBorrow;
    }

    ASSERT(!borrow);
    result->setSign(resultSign);
    return result->rightTrim(vm);
}

// Divides {x} by {divisor}, returning the result in {quotient} and {remainder}.
// Mathematically, the contract is:
// quotient = (x - remainder) / divisor, with 0 <= remainder < divisor.
// If {quotient} is an empty handle, an appropriately sized BigInt will be
// allocated for it; otherwise the caller must ensure that it is big enough.
// {quotient} can be the same as {x} for an in-place division. {quotient} can
// also be nullptr if the caller is only interested in the remainder.
void JSBigInt::absoluteDivWithDigitDivisor(VM& vm, JSBigInt* x, Digit divisor, JSBigInt** quotient, Digit& remainder)
{
    ASSERT(divisor);

    ASSERT(!x->isZero());
    remainder = 0;
    if (divisor == 1) {
        if (quotient != nullptr)
            *quotient = x;
        return;
    }

    unsigned length = x->length();
    if (quotient != nullptr) {
        if (*quotient == nullptr)
            *quotient = JSBigInt::createWithLength(vm, length);

        for (int i = length - 1; i >= 0; i--) {
            Digit q = digitDiv(remainder, x->digit(i), divisor, remainder);
            (*quotient)->setDigit(i, q);
        }
    } else {
        for (int i = length - 1; i >= 0; i--)
            digitDiv(remainder, x->digit(i), divisor, remainder);
    }
}

// Divides {dividend} by {divisor}, returning the result in {quotient} and
// {remainder}. Mathematically, the contract is:
// quotient = (dividend - remainder) / divisor, with 0 <= remainder < divisor.
// Both {quotient} and {remainder} are optional, for callers that are only
// interested in one of them.
// See Knuth, Volume 2, section 4.3.1, Algorithm D.
void JSBigInt::absoluteDivWithBigIntDivisor(VM& vm, JSBigInt* dividend, JSBigInt* divisor, JSBigInt** quotient, JSBigInt** remainder)
{
    ASSERT(divisor->length() >= 2);
    ASSERT(dividend->length() >= divisor->length());

    // The unusual variable names inside this function are consistent with
    // Knuth's book, as well as with Go's implementation of this algorithm.
    // Maintaining this consistency is probably more useful than trying to
    // come up with more descriptive names for them.
    unsigned n = divisor->length();
    unsigned m = dividend->length() - n;
    
    // The quotient to be computed.
    JSBigInt* q = nullptr;
    if (quotient != nullptr)
        q = createWithLength(vm, m + 1);
    
    // In each iteration, {qhatv} holds {divisor} * {current quotient digit}.
    // "v" is the book's name for {divisor}, "qhat" the current quotient digit.
    JSBigInt* qhatv = createWithLength(vm, n + 1);
    
    // D1.
    // Left-shift inputs so that the divisor's MSB is set. This is necessary
    // to prevent the digit-wise divisions (see digit_div call below) from
    // overflowing (they take a two digits wide input, and return a one digit
    // result).
    Digit lastDigit = divisor->digit(n - 1);
    unsigned shift = sizeof(lastDigit) == 8 ? clz64(lastDigit) : clz32(lastDigit);

    if (shift > 0)
        divisor = absoluteLeftShiftAlwaysCopy(vm, divisor, shift, LeftShiftMode::SameSizeResult);

    // Holds the (continuously updated) remaining part of the dividend, which
    // eventually becomes the remainder.
    JSBigInt* u = absoluteLeftShiftAlwaysCopy(vm, dividend, shift, LeftShiftMode::AlwaysAddOneDigit);

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
        internalMultiplyAdd(divisor, qhat, 0, n, qhatv);
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

// Returns whether (factor1 * factor2) > (high << kDigitBits) + low.
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
JSBigInt* JSBigInt::absoluteLeftShiftAlwaysCopy(VM& vm, JSBigInt* x, unsigned shift, LeftShiftMode mode)
{
    ASSERT(shift < digitBits);
    ASSERT(!x->isZero());

    unsigned n = x->length();
    unsigned resultLength = mode == LeftShiftMode::AlwaysAddOneDigit ? n + 1 : n;
    JSBigInt* result = createWithLength(vm, resultLength);

    if (!shift) {
        for (unsigned i = 0; i < n; i++)
            result->setDigit(i, x->digit(i));
        if (mode == LeftShiftMode::AlwaysAddOneDigit)
            result->setDigit(n, 0);

        return result;
    }

    Digit carry = 0;
    for (unsigned i = 0; i < n; i++) {
        Digit d = x->digit(i);
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
// remaining digits in the longer input (if {symmetric} == Symmetric, in
// {x} otherwise) are handled: copied to the result or ignored.
// Example:
//       y:             [ y2 ][ y1 ][ y0 ]
//       x:       [ x3 ][ x2 ][ x1 ][ x0 ]
//                   |     |     |     |
//                (Copy)  (op)  (op)  (op)
//                   |     |     |     |
//                   v     v     v     v
// result: [  0 ][ x3 ][ r2 ][ r1 ][ r0 ]
template<typename BitwiseOp>
inline JSBigInt* JSBigInt::absoluteBitwiseOp(VM& vm, JSBigInt* x, JSBigInt* y, ExtraDigitsHandling extraDigits, SymmetricOp symmetric, BitwiseOp&& op)
{
    unsigned xLength = x->length();
    unsigned yLength = y->length();
    unsigned numPairs = yLength;
    if (xLength < yLength) {
        numPairs = xLength;
        if (symmetric == SymmetricOp::Symmetric) {
            std::swap(x, y);
            std::swap(xLength, yLength);
        }
    }

    ASSERT(numPairs == std::min(xLength, yLength));
    unsigned resultLength = extraDigits == ExtraDigitsHandling::Copy ? xLength : numPairs;
    JSBigInt* result = createWithLength(vm, resultLength);

    unsigned i = 0;
    for (; i < numPairs; i++)
        result->setDigit(i, op(x->digit(i), y->digit(i)));

    if (extraDigits == ExtraDigitsHandling::Copy) {
        for (; i < xLength; i++)
            result->setDigit(i, x->digit(i));
    }

    for (; i < resultLength; i++)
        result->setDigit(i, 0);

    return result->rightTrim(vm);
}

JSBigInt* JSBigInt::absoluteAnd(VM& vm, JSBigInt* x, JSBigInt* y)
{
    auto digitOperation = [](Digit a, Digit b) {
        return a & b;
    };
    return absoluteBitwiseOp(vm, x, y, ExtraDigitsHandling::Skip, SymmetricOp::Symmetric, digitOperation);
}

JSBigInt* JSBigInt::absoluteOr(VM& vm, JSBigInt* x, JSBigInt* y)
{
    auto digitOperation = [](Digit a, Digit b) {
        return a | b;
    };
    return absoluteBitwiseOp(vm, x, y, ExtraDigitsHandling::Copy, SymmetricOp::Symmetric, digitOperation);
}

JSBigInt* JSBigInt::absoluteAndNot(VM& vm, JSBigInt* x, JSBigInt* y)
{
    auto digitOperation = [](Digit a, Digit b) {
        return a & ~b;
    };
    return absoluteBitwiseOp(vm, x, y, ExtraDigitsHandling::Copy, SymmetricOp::NotSymmetric, digitOperation);
}

JSBigInt* JSBigInt::absoluteAddOne(VM& vm, JSBigInt* x, SignOption signOption)
{
    unsigned inputLength = x->length();
    // The addition will overflow into a new digit if all existing digits are
    // at maximum.
    bool willOverflow = true;
    for (unsigned i = 0; i < inputLength; i++) {
        if (std::numeric_limits<Digit>::max() != x->digit(i)) {
            willOverflow = false;
            break;
        }
    }

    unsigned resultLength = inputLength + willOverflow;
    JSBigInt* result = createWithLength(vm, resultLength);

    Digit carry = 1;
    for (unsigned i = 0; i < inputLength; i++) {
        Digit newCarry = 0;
        result->setDigit(i, digitAdd(x->digit(i), carry, newCarry));
        carry = newCarry;
    }
    if (resultLength > inputLength)
        result->setDigit(inputLength, carry);
    else
        ASSERT(!carry);

    result->setSign(signOption == SignOption::Signed);
    return result->rightTrim(vm);
}

// Like the above, but you can specify that the allocated result should have
// length {resultLength}, which must be at least as large as {x->length()}.
JSBigInt* JSBigInt::absoluteSubOne(VM& vm, JSBigInt* x, unsigned resultLength)
{
    ASSERT(!x->isZero());
    ASSERT(resultLength >= x->length());
    JSBigInt* result = createWithLength(vm, resultLength);

    unsigned length = x->length();
    Digit borrow = 1;
    for (unsigned i = 0; i < length; i++) {
        Digit newBorrow = 0;
        result->setDigit(i, digitSub(x->digit(i), borrow, newBorrow));
        borrow = newBorrow;
    }
    ASSERT(!borrow);
    for (unsigned i = length; i < resultLength; i++)
        result->setDigit(i, borrow);

    return result->rightTrim(vm);
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
    unsigned leadingZeros;
    if (sizeof(lastDigit) == 8)
        leadingZeros = clz64(lastDigit);
    else
        leadingZeros = clz32(lastDigit);

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

String JSBigInt::toStringBasePowerOfTwo(ExecState* exec, JSBigInt* x, unsigned radix)
{
    ASSERT(hasOneBitSet(radix));
    ASSERT(radix >= 2 && radix <= 32);
    ASSERT(!x->isZero());
    VM& vm = exec->vm();

    const unsigned length = x->length();
    const bool sign = x->sign();
    const unsigned bitsPerChar = ctz32(radix);
    const unsigned charMask = radix - 1;
    // Compute the length of the resulting string: divide the bit length of the
    // BigInt by the number of bits representable per character (rounding up).
    const Digit msd = x->digit(length - 1);

#if USE(JSVALUE64)
    const unsigned msdLeadingZeros = clz64(msd);
#else
    const unsigned msdLeadingZeros = clz32(msd);
#endif
    
    const size_t bitLength = length * digitBits - msdLeadingZeros;
    const size_t charsRequired = (bitLength + bitsPerChar - 1) / bitsPerChar + sign;

    if (charsRequired > JSString::MaxLength) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(exec, scope);
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

String JSBigInt::toStringGeneric(ExecState* exec, JSBigInt* x, unsigned radix)
{
    // FIXME: [JSC] Revisit usage of Vector into JSBigInt::toString
    // https://bugs.webkit.org/show_bug.cgi?id=18067
    Vector<LChar> resultString;

    VM& vm = exec->vm();

    ASSERT(radix >= 2 && radix <= 36);
    ASSERT(!x->isZero());

    unsigned length = x->length();
    bool sign = x->sign();

    uint8_t maxBitsPerChar = maxBitsPerCharTable[radix];
    uint64_t maximumCharactersRequired = calculateMaximumCharactersRequired(length, radix, x->digit(length - 1), sign);

    if (maximumCharactersRequired > JSString::MaxLength) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(exec, scope);
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
            absoluteDivWithDigitDivisor(vm, *dividend, chunkDivisor, &rest, chunk);
            ASSERT(rest);

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

JSBigInt* JSBigInt::rightTrim(VM& vm)
{
    if (isZero()) {
        ASSERT(!sign());
        return this;
    }

    int nonZeroIndex = m_length - 1;
    while (nonZeroIndex >= 0 && !digit(nonZeroIndex))
        nonZeroIndex--;

    if (nonZeroIndex < 0)
        return createZero(vm);

    if (nonZeroIndex == static_cast<int>(m_length - 1))
        return this;

    unsigned newLength = nonZeroIndex + 1;
    JSBigInt* trimmedBigInt = createWithLength(vm, newLength);
    RELEASE_ASSERT(trimmedBigInt);
    std::copy(dataStorage(), dataStorage() + newLength, trimmedBigInt->dataStorage()); 

    trimmedBigInt->setSign(this->sign());

    return trimmedBigInt;
}

JSBigInt* JSBigInt::allocateFor(ExecState* exec, VM& vm, unsigned radix, unsigned charcount)
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
            if (length <= maxLength) {
                JSBigInt* result = JSBigInt::createWithLength(vm, length);
                return result;
            }
        }
    }

    if (exec) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(exec, scope);
    }
    return nullptr;
}

size_t JSBigInt::estimatedSize(JSCell* cell, VM& vm)
{
    return Base::estimatedSize(cell, vm) + jsCast<JSBigInt*>(cell)->m_length * sizeof(Digit);
}

double JSBigInt::toNumber(ExecState* exec) const
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(exec, scope, "Conversion from 'BigInt' to 'number' is not allowed."_s);
    return 0.0;
}

bool JSBigInt::getPrimitiveNumber(ExecState* exec, double& number, JSValue& result) const
{
    result = this;
    number = toNumber(exec);
    return true;
}

inline size_t JSBigInt::offsetOfData()
{
    return WTF::roundUpToMultipleOf<sizeof(Digit)>(sizeof(JSBigInt));
}

template <typename CharType>
JSBigInt* JSBigInt::parseInt(ExecState* exec, CharType*  data, unsigned length, ErrorParseMode errorParseMode)
{
    VM& vm = exec->vm();

    unsigned p = 0;
    while (p < length && isStrWhiteSpace(data[p]))
        ++p;

    // Check Radix from frist characters
    if (static_cast<unsigned>(p) + 1 < static_cast<unsigned>(length) && data[p] == '0') {
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'b'))
            return parseInt(exec, vm, data, length, p + 2, 2, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'x'))
            return parseInt(exec, vm, data, length, p + 2, 16, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'o'))
            return parseInt(exec, vm, data, length, p + 2, 8, errorParseMode, ParseIntSign::Unsigned, ParseIntMode::DisallowEmptyString);
    }

    ParseIntSign sign = ParseIntSign::Unsigned;
    if (p < length) {
        if (data[p] == '+')
            ++p;
        else if (data[p] == '-') {
            sign = ParseIntSign::Signed;
            ++p;
        }
    }

    JSBigInt* result = parseInt(exec, vm, data, length, p, 10, errorParseMode, sign);

    if (result && !result->isZero())
        result->setSign(sign == ParseIntSign::Signed);

    return result;
}

template <typename CharType>
JSBigInt* JSBigInt::parseInt(ExecState* exec, VM& vm, CharType* data, unsigned length, unsigned startIndex, unsigned radix, ErrorParseMode errorParseMode, ParseIntSign sign, ParseIntMode parseMode)
{
    ASSERT(length >= 0);
    unsigned p = startIndex;

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (parseMode != ParseIntMode::AllowEmptyString && startIndex == length) {
        ASSERT(exec);
        if (errorParseMode == ErrorParseMode::ThrowExceptions)
            throwVMError(exec, scope, createSyntaxError(exec, "Failed to parse String to BigInt"));
        return nullptr;
    }

    // Skipping leading zeros
    while (p < length && data[p] == '0')
        ++p;

    int endIndex = length - 1;
    // Removing trailing spaces
    while (endIndex >= static_cast<int>(p) && isStrWhiteSpace(data[endIndex]))
        --endIndex;

    length = endIndex + 1;

    if (p == length)
        return createZero(vm);

    unsigned limit0 = '0' + (radix < 10 ? radix : 10);
    unsigned limita = 'a' + (radix - 10);
    unsigned limitA = 'A' + (radix - 10);

    JSBigInt* result = allocateFor(exec, vm, radix, length - p);
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->initialize(InitializationType::WithZero);

    for (unsigned i = p; i < length; i++, p++) {
        uint32_t digit;
        if (data[i] >= '0' && data[i] < limit0)
            digit = data[i] - '0';
        else if (data[i] >= 'a' && data[i] < limita)
            digit = data[i] - 'a' + 10;
        else if (data[i] >= 'A' && data[i] < limitA)
            digit = data[i] - 'A' + 10;
        else
            break;

        result->inplaceMultiplyAdd(static_cast<Digit>(radix), static_cast<Digit>(digit));
    }

    result->setSign(sign == ParseIntSign::Signed ? true : false);
    if (p == length)
        return result->rightTrim(vm);

    ASSERT(exec);
    if (errorParseMode == ErrorParseMode::ThrowExceptions)
        throwVMError(exec, scope, createSyntaxError(exec, "Failed to parse String to BigInt"));

    return nullptr;
}

inline JSBigInt::Digit* JSBigInt::dataStorage()
{
    return reinterpret_cast<Digit*>(reinterpret_cast<char*>(this) + offsetOfData());
}

inline JSBigInt::Digit JSBigInt::digit(unsigned n)
{
    ASSERT(n < length());
    return dataStorage()[n];
}

inline void JSBigInt::setDigit(unsigned n, Digit value)
{
    ASSERT(n < length());
    dataStorage()[n] = value;
}
JSObject* JSBigInt::toObject(ExecState* exec, JSGlobalObject* globalObject) const
{
    return BigIntObject::create(exec->vm(), globalObject, const_cast<JSBigInt*>(this));
}

bool JSBigInt::equalsToNumber(JSValue numValue)
{
    ASSERT(numValue.isNumber());
    
    if (numValue.isInt32()) {
        int value = numValue.asInt32();
        if (!value)
            return this->isZero();

        return (this->length() == 1) && (this->sign() == (value < 0)) && (this->digit(0) == static_cast<Digit>(std::abs(static_cast<int64_t>(value))));
    }
    
    double value = numValue.asDouble();
    return compareToDouble(this, value) == ComparisonResult::Equal;
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
    int msdLeadingZeros = sizeof(xMSD) == 8  ? clz64(xMSD) : clz32(xMSD);

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

} // namespace JSC
