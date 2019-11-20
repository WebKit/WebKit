/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "CPU.h"
#include "ExceptionHelpers.h"
#include "JSObject.h"
#include <wtf/CagedUniquePtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSBigInt final : public JSCell {
public:
    using Base = JSCell;
    using Digit = UCPURegister;

    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal | OverridesToThis;
    friend class CachedBigInt;

    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.bigIntSpace;
    }

    enum class InitializationType { None, WithZero };
    void initialize(InitializationType);

    static size_t estimatedSize(JSCell*, VM&);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);
    static JSBigInt* createZero(VM&);
    static JSBigInt* tryCreateWithLength(JSGlobalObject*, unsigned length);
    static JSBigInt* createWithLengthUnchecked(VM&, unsigned length);

    static JSBigInt* createFrom(VM&, int32_t value);
    static JSBigInt* createFrom(VM&, uint32_t value);
    static JSBigInt* createFrom(VM&, int64_t value);
    static JSBigInt* createFrom(VM&, bool value);

    static size_t offsetOfLength()
    {
        return OBJECT_OFFSETOF(JSBigInt, m_length);
    }

    DECLARE_EXPORT_INFO;

    JSValue toPrimitive(JSGlobalObject*, PreferredPrimitiveType) const;

    void setSign(bool sign) { m_sign = sign; }
    bool sign() const { return m_sign; }

    unsigned length() const { return m_length; }

    enum class ErrorParseMode {
        ThrowExceptions,
        IgnoreExceptions
    };

    enum class ParseIntMode { DisallowEmptyString, AllowEmptyString };
    enum class ParseIntSign { Unsigned, Signed };

    static JSBigInt* parseInt(JSGlobalObject*, VM&, StringView, uint8_t radix, ErrorParseMode = ErrorParseMode::ThrowExceptions, ParseIntSign = ParseIntSign::Unsigned);
    static JSBigInt* parseInt(JSGlobalObject*, StringView, ErrorParseMode = ErrorParseMode::ThrowExceptions);
    static JSBigInt* stringToBigInt(JSGlobalObject*, StringView);

    static String tryGetString(VM&, JSBigInt*, unsigned radix);

    Optional<uint8_t> singleDigitValueForString();
    String toString(JSGlobalObject*, unsigned radix);
    
    enum class ComparisonMode {
        LessThan,
        LessThanOrEqual
    };

    enum class ComparisonResult {
        Equal,
        Undefined,
        GreaterThan,
        LessThan
    };
    
    JS_EXPORT_PRIVATE static bool equals(JSBigInt*, JSBigInt*);
    bool equalsToNumber(JSValue);
    static ComparisonResult compare(JSBigInt* x, JSBigInt* y);

    bool getPrimitiveNumber(JSGlobalObject*, double& number, JSValue& result) const;
    double toNumber(JSGlobalObject*) const;

    JSObject* toObject(JSGlobalObject*) const;
    inline bool toBoolean() const { return !isZero(); }

    static JSBigInt* exponentiate(JSGlobalObject*, JSBigInt* base, JSBigInt* exponent);

    static JSBigInt* multiply(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    
    ComparisonResult static compareToDouble(JSBigInt* x, double y);

    static JSBigInt* inc(JSGlobalObject*, JSBigInt* x);
    static JSBigInt* dec(JSGlobalObject*, JSBigInt* x);
    static JSBigInt* add(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* sub(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* divide(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* remainder(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* unaryMinus(VM&, JSBigInt* x);

    static JSBigInt* bitwiseAnd(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* bitwiseOr(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* bitwiseXor(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* bitwiseNot(JSGlobalObject*, JSBigInt* x);

    static JSBigInt* leftShift(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* signedRightShift(JSGlobalObject*, JSBigInt* x, JSBigInt* y);

private:
    JSBigInt(VM&, Structure*, Digit*, unsigned length);

    static constexpr unsigned bitsPerByte = 8;
    static constexpr unsigned digitBits = sizeof(Digit) * bitsPerByte;
    static constexpr unsigned halfDigitBits = digitBits / 2;
    static constexpr Digit halfDigitMask = (1ull << halfDigitBits) - 1;
    static constexpr int maxInt = 0x7FFFFFFF;
    
    // The maximum length that the current implementation supports would be
    // maxInt / digitBits. However, we use a lower limit for now, because
    // raising it later is easier than lowering it.
    // Support up to 1 million bits.
    static constexpr unsigned maxLength = 1024 * 1024 / (sizeof(void*) * bitsPerByte);
    static constexpr unsigned maxLengthBits = maxInt - sizeof(void*) * bitsPerByte - 1;
    
    static uint64_t calculateMaximumCharactersRequired(unsigned length, unsigned radix, Digit lastDigit, bool sign);
    
    static ComparisonResult absoluteCompare(JSBigInt* x, JSBigInt* y);
    static void absoluteDivWithDigitDivisor(VM&, JSBigInt* x, Digit divisor, JSBigInt** quotient, Digit& remainder);
    static void internalMultiplyAdd(JSBigInt* source, Digit factor, Digit summand, unsigned, JSBigInt* result);
    static void multiplyAccumulate(JSBigInt* multiplicand, Digit multiplier, JSBigInt* accumulator, unsigned accumulatorIndex);
    static void absoluteDivWithBigIntDivisor(JSGlobalObject*, JSBigInt* dividend, JSBigInt* divisor, JSBigInt** quotient, JSBigInt** remainder);
    
    enum class LeftShiftMode {
        SameSizeResult,
        AlwaysAddOneDigit
    };
    
    static JSBigInt* absoluteLeftShiftAlwaysCopy(JSGlobalObject*, JSBigInt* x, unsigned shift, LeftShiftMode);
    static bool productGreaterThan(Digit factor1, Digit factor2, Digit high, Digit low);

    Digit absoluteInplaceAdd(JSBigInt* summand, unsigned startIndex);
    Digit absoluteInplaceSub(JSBigInt* subtrahend, unsigned startIndex);
    void inplaceRightShift(unsigned shift);

    enum class ExtraDigitsHandling {
        Copy,
        Skip
    };

    enum class SymmetricOp {
        Symmetric,
        NotSymmetric
    };

    template<typename BitwiseOp>
    static JSBigInt* absoluteBitwiseOp(VM&, JSBigInt* x, JSBigInt* y, ExtraDigitsHandling, SymmetricOp, BitwiseOp&&);

    static JSBigInt* absoluteAnd(VM&, JSBigInt* x, JSBigInt* y);
    static JSBigInt* absoluteOr(VM&, JSBigInt* x, JSBigInt* y);
    static JSBigInt* absoluteAndNot(VM&, JSBigInt* x, JSBigInt* y);
    static JSBigInt* absoluteXor(VM&, JSBigInt* x, JSBigInt* y);

    enum class SignOption {
        Signed,
        Unsigned
    };

    static JSBigInt* absoluteAddOne(JSGlobalObject*, JSBigInt* x, SignOption);
    static JSBigInt* absoluteSubOne(JSGlobalObject*, JSBigInt* x, unsigned resultLength);

    // Digit arithmetic helpers.
    static Digit digitAdd(Digit a, Digit b, Digit& carry);
    static Digit digitSub(Digit a, Digit b, Digit& borrow);
    static Digit digitMul(Digit a, Digit b, Digit& high);
    static Digit digitDiv(Digit high, Digit low, Digit divisor, Digit& remainder);
    static Digit digitPow(Digit base, Digit exponent);

    static String toStringBasePowerOfTwo(VM&, JSGlobalObject*, JSBigInt*, unsigned radix);
    static String toStringGeneric(VM&, JSGlobalObject*, JSBigInt*, unsigned radix);

    inline bool isZero() const
    {
        ASSERT(length() || !sign());
        return length() == 0;
    }

    template <typename CharType>
    static JSBigInt* parseInt(JSGlobalObject*, CharType*  data, unsigned length, ErrorParseMode);

    template <typename CharType>
    static JSBigInt* parseInt(JSGlobalObject*, VM&, CharType* data, unsigned length, unsigned startIndex, unsigned radix, ErrorParseMode, ParseIntSign = ParseIntSign::Signed, ParseIntMode = ParseIntMode::AllowEmptyString);

    static JSBigInt* allocateFor(JSGlobalObject*, VM&, unsigned radix, unsigned charcount);

    static JSBigInt* copy(VM&, JSBigInt* x);
    JSBigInt* rightTrim(VM&);

    void inplaceMultiplyAdd(Digit multiplier, Digit part);
    static JSBigInt* absoluteAdd(JSGlobalObject*, JSBigInt* x, JSBigInt* y, bool resultSign);
    static JSBigInt* absoluteSub(VM&, JSBigInt* x, JSBigInt* y, bool resultSign);

    static JSBigInt* leftShiftByAbsolute(JSGlobalObject*, JSBigInt* x, JSBigInt* y);
    static JSBigInt* rightShiftByAbsolute(JSGlobalObject*, JSBigInt* x, JSBigInt* y);

    static JSBigInt* rightShiftByMaximum(VM&, bool sign);

    static Optional<Digit> toShiftAmount(JSBigInt* x);

    inline static size_t offsetOfData()
    {
        return OBJECT_OFFSETOF(JSBigInt, m_data);
    }

    inline Digit* dataStorage() { return m_data.get(m_length); }

    Digit digit(unsigned);
    void setDigit(unsigned, Digit);

    const unsigned m_length;
    bool m_sign { false };
    CagedUniquePtr<Gigacage::Primitive, Digit> m_data;
};

inline JSBigInt* asBigInt(JSValue value)
{
    ASSERT(value.asCell()->isBigInt());
    return jsCast<JSBigInt*>(value.asCell());
}

} // namespace JSC
