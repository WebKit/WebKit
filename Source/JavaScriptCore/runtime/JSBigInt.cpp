/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <wtf/text/StringVector.h>

#define STATIC_ASSERT(cond) static_assert(cond, "JSBigInt assumes " #cond)

namespace JSC {

const ClassInfo JSBigInt::s_info =
    { "JSBigInt", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSBigInt) };

void JSBigInt::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSBigInt* thisObject = jsCast<JSBigInt*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

JSBigInt::JSBigInt(VM& vm, Structure* structure, int length)
    : Base(vm, structure)
    , m_length(length)
{ }

void JSBigInt::initialize(InitializationType initType)
{
    setSign(false);
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
    zeroBigInt->setSign(false);
    return zeroBigInt;
}

size_t JSBigInt::allocationSize(int length)
{
    size_t sizeWithPadding = WTF::roundUpToMultipleOf<sizeof(size_t)>(sizeof(JSBigInt));
    return sizeWithPadding + length * sizeof(Digit);
}

JSBigInt* JSBigInt::createWithLength(VM& vm, int length)
{
    JSBigInt* bigInt = new (NotNull, allocateCell<JSBigInt>(vm.heap, allocationSize(length))) JSBigInt(vm, vm.bigIntStructure.get(), length);
    bigInt->finishCreation(vm);
    return bigInt;
}

void JSBigInt::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}


JSBigInt* JSBigInt::createFrom(VM& vm, int32_t value)
{
    if (!value)
        return createZero(vm);
    
    JSBigInt* bigInt = createWithLength(vm, 1);
    
    if (value < 0) {
        bigInt->setDigit(0, static_cast<Digit>(-1 * static_cast<int64_t>(value)));
        bigInt->setSign(true);
    } else {
        bigInt->setDigit(0, static_cast<Digit>(value));
        bigInt->setSign(false);
    }

    return bigInt;
}

JSBigInt* JSBigInt::createFrom(VM& vm, uint32_t value)
{
    if (!value)
        return createZero(vm);
    
    JSBigInt* bigInt = createWithLength(vm, 1);
    bigInt->setDigit(0, static_cast<Digit>(value));
    bigInt->setSign(false);
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
        } else {
            bigInt->setDigit(0, static_cast<Digit>(value));
            bigInt->setSign(false);
        }
        
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
    bigInt->setSign(false);
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

JSBigInt* JSBigInt::parseInt(ExecState* state, StringView s)
{
    if (s.is8Bit())
        return parseInt(state, s.characters8(), s.length());
    return parseInt(state, s.characters16(), s.length());
}

JSBigInt* JSBigInt::parseInt(ExecState* state, VM& vm, StringView s, uint8_t radix)
{
    if (s.is8Bit())
        return parseInt(state, vm, s.characters8(), s.length(), 0, radix, false);
    return parseInt(state, vm, s.characters16(), s.length(), 0, radix, false);
}

String JSBigInt::toString(ExecState& state, int radix)
{
    if (this->isZero())
        return state.vm().smallStrings.singleCharacterStringRep('0');

    return toStringGeneric(state, this, radix);
}

bool JSBigInt::isZero()
{
    ASSERT(length() || !sign());
    return length() == 0;
}

// Multiplies {this} with {factor} and adds {summand} to the result.
void JSBigInt::inplaceMultiplyAdd(uintptr_t factor, uintptr_t summand)
{
    STATIC_ASSERT(sizeof(factor) == sizeof(Digit));
    STATIC_ASSERT(sizeof(summand) == sizeof(Digit));

    internalMultiplyAdd(this, factor, summand, length(), this);
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
JSBigInt::Digit JSBigInt::digitAdd(Digit a, Digit b, Digit& carry)
{
#if HAVE_TWO_DIGIT
    TwoDigit result = static_cast<TwoDigit>(a) + static_cast<TwoDigit>(b);
    carry += result >> digitBits;

    return static_cast<Digit>(result);
#else
    Digit result = a + b;

    if (result < a)
        carry += 1;
    
    return result;
#endif
}

// {borrow} must point to an initialized Digit and will either be incremented
// by one or left alone.
JSBigInt::Digit JSBigInt::digitSub(Digit a, Digit b, Digit& borrow)
{
#if HAVE_TWO_DIGIT
    TwoDigit result = static_cast<TwoDigit>(a) - static_cast<TwoDigit>(b);
    borrow += (result >> digitBits) & 1;
    
    return static_cast<Digit>(result);
#else
    Digit result = a - b;
    if (result > a)
        borrow += 1;
    
    return static_cast<Digit>(result);
#endif
}

// Returns the low half of the result. High half is in {high}.
JSBigInt::Digit JSBigInt::digitMul(Digit a, Digit b, Digit& high)
{
#if HAVE_TWO_DIGIT
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
JSBigInt::Digit JSBigInt::digitPow(Digit base, Digit exponent)
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
JSBigInt::Digit JSBigInt::digitDiv(Digit high, Digit low, Digit divisor, Digit& remainder)
{
    ASSERT(high < divisor);
#if CPU(X86_64) && COMPILER(GCC_OR_CLANG)
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
#elif CPU(X86) && COMPILER(GCC_OR_CLANG)
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
    static const Digit kHalfDigitBase = 1ull << halfDigitBits;
    // Adapted from Warren, Hacker's Delight, p. 152.
#if USE(JSVALUE64)
    int s = clz64(divisor);
#else
    int s = clz32(divisor);
#endif
    divisor <<= s;
    
    Digit vn1 = divisor >> halfDigitBits;
    Digit vn0 = divisor & halfDigitMask;

    // {s} can be 0. "low >> digitBits == low" on x86, so we "&" it with
    // {s_zero_mask} which is 0 if s == 0 and all 1-bits otherwise.
    STATIC_ASSERT(sizeof(intptr_t) == sizeof(Digit));
    Digit sZeroMask = static_cast<Digit>(static_cast<intptr_t>(-s) >> (digitBits - 1));
    Digit un32 = (high << s) | ((low >> (digitBits - s)) & sZeroMask);
    Digit un10 = low << s;
    Digit un1 = un10 >> halfDigitBits;
    Digit un0 = un10 & halfDigitMask;
    Digit q1 = un32 / vn1;
    Digit rhat = un32 - q1 * vn1;

    while (q1 >= kHalfDigitBase || q1 * vn0 > rhat * kHalfDigitBase + un1) {
        q1--;
        rhat += vn1;
        if (rhat >= kHalfDigitBase)
            break;
    }

    Digit un21 = un32 * kHalfDigitBase + un1 - q1 * divisor;
    Digit q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;

    while (q0 >= kHalfDigitBase || q0 * vn0 > rhat * kHalfDigitBase + un0) {
        q0--;
        rhat += vn1;
        if (rhat >= kHalfDigitBase)
            break;
    }

    remainder = (un21 * kHalfDigitBase + un0 - q0 * divisor) >> s;
    return q1 * kHalfDigitBase + q0;
#endif
}

// Multiplies {source} with {factor} and adds {summand} to the result.
// {result} and {source} may be the same BigInt for inplace modification.
void JSBigInt::internalMultiplyAdd(JSBigInt* source, Digit factor, Digit summand, int n, JSBigInt* result)
{
    ASSERT(source->length() >= n);
    ASSERT(result->length() >= n);

    Digit carry = summand;
    Digit high = 0;
    for (int i = 0; i < n; i++) {
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

bool JSBigInt::equals(JSBigInt* x, JSBigInt* y)
{
    if (x->sign() != y->sign())
        return false;

    if (x->length() != y->length())
        return false;

    for (int i = 0; i < x->length(); i++) {
        if (x->digit(i) != y->digit(i))
            return false;
    }

    return true;
}

// Divides {x} by {divisor}, returning the result in {quotient} and {remainder}.
// Mathematically, the contract is:
// quotient = (x - remainder) / divisor, with 0 <= remainder < divisor.
// If {quotient} is an empty handle, an appropriately sized BigInt will be
// allocated for it; otherwise the caller must ensure that it is big enough.
// {quotient} can be the same as {x} for an in-place division. {quotient} can
// also be nullptr if the caller is only interested in the remainder.
void JSBigInt::absoluteDivSmall(ExecState& state, JSBigInt* x, Digit divisor, JSBigInt** quotient, Digit& remainder)
{
    ASSERT(divisor);

    VM& vm = state.vm();
    ASSERT(!x->isZero());
    remainder = 0;
    if (divisor == 1) {
        if (quotient != nullptr)
            *quotient = x;
        return;
    }

    int length = x->length();
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

static const int bitsPerCharTableShift = 5;
static const size_t bitsPerCharTableMultiplier = 1u << bitsPerCharTableShift;

// Compute (an overapproximation of) the length of the resulting string:
// Divide bit length of the BigInt by bits representable per character.
uint64_t JSBigInt::calculateMaximumCharactersRequired(int length, int radix, Digit lastDigit, bool sign)
{
    int leadingZeros;
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

String JSBigInt::toStringGeneric(ExecState& state, JSBigInt* x, int radix)
{
    // FIXME: [JSC] Revisit usage of StringVector into JSBigInt::toString
    // https://bugs.webkit.org/show_bug.cgi?id=18067
    StringVector<LChar> resultString;

    ASSERT(radix >= 2 && radix <= 36);
    ASSERT(!x->isZero());

    int length = x->length();
    bool sign = x->sign();

    uint8_t maxBitsPerChar = maxBitsPerCharTable[radix];
    uint64_t maximumCharactersRequired = calculateMaximumCharactersRequired(length, radix, x->digit(length - 1), sign);

    if (maximumCharactersRequired > JSString::MaxLength) {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        throwOutOfMemoryError(&state, scope);
        return String();
    }

    Digit lastDigit;
    if (length == 1)
        lastDigit = x->digit(0);
    else {
        int chunkChars = digitBits * bitsPerCharTableMultiplier / maxBitsPerChar;
        Digit chunkDivisor = digitPow(radix, chunkChars);

        // By construction of chunkChars, there can't have been overflow.
        ASSERT(chunkDivisor);
        int nonZeroDigit = length - 1;
        ASSERT(x->digit(nonZeroDigit));

        // {rest} holds the part of the BigInt that we haven't looked at yet.
        // Not to be confused with "remainder"!
        JSBigInt* rest = nullptr;

        // In the first round, divide the input, allocating a new BigInt for
        // the result == rest; from then on divide the rest in-place.
        JSBigInt** dividend = &x;
        do {
            Digit chunk;
            absoluteDivSmall(state, *dividend, chunkDivisor, &rest, chunk);
            ASSERT(rest);

            dividend = &rest;
            for (int i = 0; i < chunkChars; i++) {
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
    int newSizeNoLeadingZeroes = resultString.size();
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
    if (isZero())
        return this;

    int nonZeroIndex = m_length - 1;
    while (nonZeroIndex >= 0 && !digit(nonZeroIndex))
        nonZeroIndex--;

    if (nonZeroIndex == m_length - 1)
        return this;

    int newLength = nonZeroIndex + 1;
    JSBigInt* trimmedBigInt = createWithLength(vm, newLength);
    RELEASE_ASSERT(trimmedBigInt);
    std::copy(dataStorage(), dataStorage() + newLength, trimmedBigInt->dataStorage()); 

    trimmedBigInt->setSign(this->sign());

    return trimmedBigInt;
}

JSBigInt* JSBigInt::allocateFor(ExecState* state, VM& vm, int radix, int charcount)
{
    ASSERT(2 <= radix && radix <= 36);
    ASSERT(charcount >= 0);

    size_t bitsPerChar = maxBitsPerCharTable[radix];
    size_t chars = charcount;
    const int roundup = bitsPerCharTableMultiplier - 1;
    if (chars <= (std::numeric_limits<size_t>::max() - roundup) / bitsPerChar) {
        size_t bitsMin = bitsPerChar * chars;

        // Divide by 32 (see table), rounding up.
        bitsMin = (bitsMin + roundup) >> bitsPerCharTableShift;
        if (bitsMin <= static_cast<size_t>(maxInt)) {
            // Divide by kDigitsBits, rounding up.
            int length = (static_cast<int>(bitsMin) + digitBits - 1) / digitBits;
            if (length <= maxLength) {
                JSBigInt* result = JSBigInt::createWithLength(vm, length);
                return result;
            }
        }
    }

    if (state) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(state, scope);
    }
    return nullptr;
}

size_t JSBigInt::estimatedSize(JSCell* cell)
{
    return Base::estimatedSize(cell) + jsCast<JSBigInt*>(cell)->m_length * sizeof(Digit);
}

double JSBigInt::toNumber(ExecState* state) const
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(state, scope, ASCIILiteral("Convertion from 'BigInt' to 'number' is not allowed."));
    return 0.0;
}

bool JSBigInt::getPrimitiveNumber(ExecState* state, double& number, JSValue& result) const
{
    result = this;
    number = toNumber(state);
    return true;
}

size_t JSBigInt::offsetOfData()
{
    return WTF::roundUpToMultipleOf<sizeof(Digit)>(sizeof(JSBigInt));
}

template <typename CharType>
JSBigInt* JSBigInt::parseInt(ExecState* state, CharType*  data, int length)
{
    VM& vm = state->vm();

    int p = 0;
    while (p < length && isStrWhiteSpace(data[p]))
        ++p;

    // Check Radix from frist characters
    if (p + 1 < length && data[p] == '0') {
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'b'))
            return parseInt(state, vm, data, length, p + 2, 2, false);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'x'))
            return parseInt(state, vm, data, length, p + 2, 16, false);
        
        if (isASCIIAlphaCaselessEqual(data[p + 1], 'o'))
            return parseInt(state, vm, data, length, p + 2, 8, false);
    }

    bool sign = false;
    if (p < length) {
        if (data[p] == '+')
            ++p;
        else if (data[p] == '-') {
            sign = true;
            ++p;
        }
    }

    JSBigInt* result = parseInt(state, vm, data, length, p, 10);

    if (result && !result->isZero())
        result->setSign(sign);

    return result;
}

template <typename CharType>
JSBigInt* JSBigInt::parseInt(ExecState* state, VM& vm, CharType* data, int length, int startIndex, int radix, bool allowEmptyString)
{
    ASSERT(length >= 0);
    int p = startIndex;

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!allowEmptyString && startIndex == length) {
        ASSERT(state);
        throwVMError(state, scope, createSyntaxError(state, "Failed to parse String to BigInt"));
        return nullptr;
    }

    // Skipping leading zeros
    while (p < length && data[p] == '0')
        ++p;

    int endIndex = length - 1;
    // Removing trailing spaces
    while (endIndex >= p && isStrWhiteSpace(data[endIndex]))
        --endIndex;

    length = endIndex + 1;

    if (p == length)
        return createZero(vm);

    int limit0 = '0' + (radix < 10 ? radix : 10);
    int limita = 'a' + (radix - 10);
    int limitA = 'A' + (radix - 10);

    JSBigInt* result = allocateFor(state, vm, radix, length - p);
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->initialize(InitializationType::WithZero);

    for (int i = p; i < length; i++, p++) {
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

    if (p == length)
        return result->rightTrim(vm);

    ASSERT(state);
    throwVMError(state, scope, createSyntaxError(state, "Failed to parse String to BigInt"));

    return nullptr;
}

JSBigInt::Digit* JSBigInt::dataStorage()
{
    return reinterpret_cast<Digit*>(reinterpret_cast<char*>(this) + offsetOfData());
}

JSBigInt::Digit JSBigInt::digit(int n)
{
    ASSERT(n >= 0 && n < length());
    return dataStorage()[n];
}

void JSBigInt::setDigit(int n, Digit value)
{
    ASSERT(n >= 0 && n < length());
    dataStorage()[n] = value;
}

JSObject* JSBigInt::toObject(ExecState* exec, JSGlobalObject* globalObject) const
{
    return BigIntObject::create(exec->vm(), globalObject, const_cast<JSBigInt*>(this));
}

} // namespace JSC
