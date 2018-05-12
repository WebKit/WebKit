/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
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

#include "ExceptionHelpers.h"
#include "JSObject.h"
#include "ParseInt.h"
#include <wtf/CagedPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSBigInt final : public JSCell {
    using Base = JSCell;
    static const unsigned StructureFlags = Base::StructureFlags | OverridesToThis;

public:

    JSBigInt(VM&, Structure*, unsigned length);

    enum class InitializationType { None, WithZero };
    void initialize(InitializationType);

    static void visitChildren(JSCell*, SlotVisitor&);

    static size_t estimatedSize(JSCell*);
    static size_t allocationSize(unsigned length);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);
    static JSBigInt* createZero(VM&);
    static JSBigInt* createWithLength(VM&, unsigned length);

    static JSBigInt* createFrom(VM&, int32_t value);
    static JSBigInt* createFrom(VM&, uint32_t value);
    static JSBigInt* createFrom(VM&, int64_t value);
    static JSBigInt* createFrom(VM&, bool value);

    DECLARE_EXPORT_INFO;

    void finishCreation(VM&);

    JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;

    void setSign(bool sign) { m_sign = sign; }
    bool sign() const { return m_sign; }

    void setLength(unsigned length) { m_length = length; }
    unsigned length() const { return m_length; }

    enum ErrorParseMode {
        ThrowExceptions,
        IgnoreExceptions
    };

    static JSBigInt* parseInt(ExecState*, VM&, StringView, uint8_t radix, ErrorParseMode = ThrowExceptions);
    static JSBigInt* parseInt(ExecState*, StringView, ErrorParseMode = ThrowExceptions);
    static JSBigInt* stringToBigInt(ExecState*, StringView);

    std::optional<uint8_t> singleDigitValueForString();
    String toString(ExecState&, unsigned radix);
    
    JS_EXPORT_PRIVATE static bool equals(JSBigInt*, JSBigInt*);
    bool equalsToNumber(JSValue);

    bool getPrimitiveNumber(ExecState*, double& number, JSValue& result) const;
    double toNumber(ExecState*) const;

    JSObject* toObject(ExecState*, JSGlobalObject*) const;

    static JSBigInt* multiply(ExecState*, JSBigInt* x, JSBigInt* y);
    
private:

    enum ComparisonResult {
        Equal,
        Undefined,
        GreaterThan,
        LessThan
    };

    using Digit = uintptr_t;
    static constexpr const unsigned bitsPerByte = 8;
    static constexpr const unsigned digitBits = sizeof(Digit) * bitsPerByte;
    static constexpr const unsigned halfDigitBits = digitBits / 2;
    static constexpr const Digit halfDigitMask = (1ull << halfDigitBits) - 1;
    static constexpr const int maxInt = 0x7FFFFFFF;
    
    // The maximum length that the current implementation supports would be
    // maxInt / digitBits. However, we use a lower limit for now, because
    // raising it later is easier than lowering it.
    // Support up to 1 million bits.
    static const unsigned maxLength = 1024 * 1024 / (sizeof(void*) * bitsPerByte);
    
    static uint64_t calculateMaximumCharactersRequired(unsigned length, unsigned radix, Digit lastDigit, bool sign);
    
    static void absoluteDivSmall(ExecState&, JSBigInt* x, Digit divisor, JSBigInt** quotient, Digit& remainder);
    static void internalMultiplyAdd(JSBigInt* source, Digit factor, Digit summand, unsigned, JSBigInt* result);
    static void multiplyAccumulate(JSBigInt* multiplicand, Digit multiplier, JSBigInt* accumulator, unsigned accumulatorIndex);

    // Digit arithmetic helpers.
    static Digit digitAdd(Digit a, Digit b, Digit& carry);
    static Digit digitSub(Digit a, Digit b, Digit& borrow);
    static Digit digitMul(Digit a, Digit b, Digit& high);
    static Digit digitDiv(Digit high, Digit low, Digit divisor, Digit& remainder);
    static Digit digitPow(Digit base, Digit exponent);

    static String toStringGeneric(ExecState&, JSBigInt*, unsigned radix);

    bool isZero();

    ComparisonResult static compareToDouble(JSBigInt* x, double y);

    template <typename CharType>
    static JSBigInt* parseInt(ExecState*, CharType*  data, unsigned length, ErrorParseMode);

    template <typename CharType>
    static JSBigInt* parseInt(ExecState*, VM&, CharType* data, unsigned length, unsigned startIndex, unsigned radix, ErrorParseMode, bool allowEmptyString = true);

    static JSBigInt* allocateFor(ExecState*, VM&, unsigned radix, unsigned charcount);

    JSBigInt* rightTrim(VM&);

    void inplaceMultiplyAdd(Digit multiplier, Digit part);
    
    static size_t offsetOfData();
    Digit* dataStorage();

    Digit digit(unsigned);
    void setDigit(unsigned, Digit);
        
    unsigned m_length;
    bool m_sign;
};

inline JSBigInt* asBigInt(JSValue value)
{
    ASSERT(value.asCell()->isBigInt());
    return jsCast<JSBigInt*>(value.asCell());
}

} // namespace JSC
