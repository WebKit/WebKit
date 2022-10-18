/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "BuiltinNames.h"
#include "IntlNumberFormat.h"
#include "IntlObjectInlines.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectFunctions.h"

namespace JSC  {

template<typename IntlType>
void setNumberFormatDigitOptions(JSGlobalObject* globalObject, IntlType* intlInstance, JSObject* options, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation notation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned minimumIntegerDigits = intlNumberOption(globalObject, options, vm.propertyNames->minimumIntegerDigits, 1, 21, 1);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue minimumFractionDigitsValue = jsUndefined();
    JSValue maximumFractionDigitsValue = jsUndefined();
    JSValue minimumSignificantDigitsValue = jsUndefined();
    JSValue maximumSignificantDigitsValue = jsUndefined();
    if (options) {
        minimumFractionDigitsValue = options->get(globalObject, vm.propertyNames->minimumFractionDigits);
        RETURN_IF_EXCEPTION(scope, void());

        maximumFractionDigitsValue = options->get(globalObject, vm.propertyNames->maximumFractionDigits);
        RETURN_IF_EXCEPTION(scope, void());

        minimumSignificantDigitsValue = options->get(globalObject, vm.propertyNames->minimumSignificantDigits);
        RETURN_IF_EXCEPTION(scope, void());

        maximumSignificantDigitsValue = options->get(globalObject, vm.propertyNames->maximumSignificantDigits);
        RETURN_IF_EXCEPTION(scope, void());
    }

    intlInstance->m_minimumIntegerDigits = minimumIntegerDigits;

    IntlRoundingPriority roundingPriority = intlOption<IntlRoundingPriority>(globalObject, options, vm.propertyNames->roundingPriority, { { "auto"_s, IntlRoundingPriority::Auto }, { "morePrecision"_s, IntlRoundingPriority::MorePrecision }, { "lessPrecision"_s, IntlRoundingPriority::LessPrecision } }, "roundingPriority must be either \"auto\", \"morePrecision\", or \"lessPrecision\""_s, IntlRoundingPriority::Auto);
    RETURN_IF_EXCEPTION(scope, void());

    bool hasSd = !minimumSignificantDigitsValue.isUndefined() || !maximumSignificantDigitsValue.isUndefined();
    bool hasFd = !minimumFractionDigitsValue.isUndefined() || !maximumFractionDigitsValue.isUndefined();

    bool needSd = true;
    bool needFd = true;

    if (roundingPriority == IntlRoundingPriority::Auto) {
        needSd = hasSd;
        if (hasSd || (!hasFd && notation == IntlNotation::Compact))
            needFd = false;
    }

    if (needSd) {
        if (hasSd) {
            unsigned minimumSignificantDigits = intlDefaultNumberOption(globalObject, minimumSignificantDigitsValue, vm.propertyNames->minimumSignificantDigits, 1, 21, 1);
            RETURN_IF_EXCEPTION(scope, void());
            unsigned maximumSignificantDigits = intlDefaultNumberOption(globalObject, maximumSignificantDigitsValue, vm.propertyNames->maximumSignificantDigits, minimumSignificantDigits, 21, 21);
            RETURN_IF_EXCEPTION(scope, void());
            intlInstance->m_minimumSignificantDigits = minimumSignificantDigits;
            intlInstance->m_maximumSignificantDigits = maximumSignificantDigits;
        } else {
            intlInstance->m_minimumSignificantDigits = 1;
            intlInstance->m_maximumSignificantDigits = 21;
        }
    }

    if (needFd) {
        if (hasFd) {
            constexpr unsigned undefinedValue = UINT32_MAX;
            unsigned minimumFractionDigits = intlDefaultNumberOption(globalObject, minimumFractionDigitsValue, vm.propertyNames->minimumFractionDigits, 0, 20, undefinedValue);
            RETURN_IF_EXCEPTION(scope, void());
            unsigned maximumFractionDigits = intlDefaultNumberOption(globalObject, maximumFractionDigitsValue, vm.propertyNames->maximumFractionDigits, 0, 20, undefinedValue);
            RETURN_IF_EXCEPTION(scope, void());

            if (minimumFractionDigits == undefinedValue)
                minimumFractionDigits = std::min(minimumFractionDigitsDefault, maximumFractionDigits);
            else if (maximumFractionDigits == undefinedValue)
                maximumFractionDigits = std::max(maximumFractionDigitsDefault, minimumFractionDigits);
            else if (minimumFractionDigits > maximumFractionDigits) {
                throwRangeError(globalObject, scope, "Computed minimumFractionDigits is larger than maximumFractionDigits"_s);
                return;
            }

            intlInstance->m_minimumFractionDigits = minimumFractionDigits;
            intlInstance->m_maximumFractionDigits = maximumFractionDigits;
        } else {
            intlInstance->m_minimumFractionDigits = minimumFractionDigitsDefault;
            intlInstance->m_maximumFractionDigits = maximumFractionDigitsDefault;
        }
    }

    if (needSd || needFd) {
        if (roundingPriority == IntlRoundingPriority::MorePrecision)
            intlInstance->m_roundingType = IntlRoundingType::MorePrecision;
        else if (roundingPriority == IntlRoundingPriority::LessPrecision)
            intlInstance->m_roundingType = IntlRoundingType::LessPrecision;
        else if (hasSd)
            intlInstance->m_roundingType = IntlRoundingType::SignificantDigits;
        else
            intlInstance->m_roundingType = IntlRoundingType::FractionDigits;
    } else {
        intlInstance->m_roundingType = IntlRoundingType::MorePrecision;
        intlInstance->m_minimumFractionDigits = 0;
        intlInstance->m_maximumFractionDigits = 0;
        intlInstance->m_minimumSignificantDigits = 1;
        intlInstance->m_maximumSignificantDigits = 2;
    }
}

template<typename IntlType>
void appendNumberFormatDigitOptionsToSkeleton(IntlType* intlInstance, StringBuilder& skeletonBuilder)
{
    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#integer-width
    skeletonBuilder.append(" integer-width/", WTF::ICU::majorVersion() >= 67 ? '*' : '+'); // Prior to ICU 67, use the symbol + instead of *.
    for (unsigned i = 0; i < intlInstance->m_minimumIntegerDigits; ++i)
        skeletonBuilder.append('0');

    if (intlInstance->m_roundingIncrement != 1) {
        skeletonBuilder.append(" precision-increment/");
        auto string = numberToStringUnsigned<Vector<LChar, 10>>(intlInstance->m_roundingIncrement);
        if (intlInstance->m_maximumFractionDigits >= string.size()) {
            skeletonBuilder.append("0.");
            for (unsigned i = 0; i < (intlInstance->m_maximumFractionDigits - string.size()); ++i)
                skeletonBuilder.append('0');
            skeletonBuilder.appendCharacters(string.data(), string.size());
        } else {
            unsigned nonFraction = string.size() - intlInstance->m_maximumFractionDigits;
            skeletonBuilder.appendCharacters(string.data(), nonFraction);
            skeletonBuilder.append('.');
            skeletonBuilder.appendCharacters(string.data() + nonFraction, intlInstance->m_maximumFractionDigits);
        }
    } else {
        switch (intlInstance->m_roundingType) {
        case IntlRoundingType::FractionDigits: {
            // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
            skeletonBuilder.append(" .");
            for (unsigned i = 0; i < intlInstance->m_minimumFractionDigits; ++i)
                skeletonBuilder.append('0');
            for (unsigned i = 0; i < intlInstance->m_maximumFractionDigits - intlInstance->m_minimumFractionDigits; ++i)
                skeletonBuilder.append('#');
            break;
        }
        case IntlRoundingType::SignificantDigits: {
            // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#significant-digits-precision
            skeletonBuilder.append(' ');
            for (unsigned i = 0; i < intlInstance->m_minimumSignificantDigits; ++i)
                skeletonBuilder.append('@');
            for (unsigned i = 0; i < intlInstance->m_maximumSignificantDigits - intlInstance->m_minimumSignificantDigits; ++i)
                skeletonBuilder.append('#');
            break;
        }
        case IntlRoundingType::MorePrecision:
        case IntlRoundingType::LessPrecision:
            // Before Intl.NumberFormat v3, it was CompactRounding mode, where we do not configure anything.
            // So, if linked ICU is ~68, we do nothing.
            if (WTF::ICU::majorVersion() >= 69) {
                // https://github.com/unicode-org/icu/commit/d7db6c1f8655bb53153695b09a50029fd04a8364
                // https://github.com/unicode-org/icu/blob/main/docs/userguide/format_parse/numbers/skeletons.md#precision
                skeletonBuilder.append(" .");
                for (unsigned i = 0; i < intlInstance->m_minimumFractionDigits; ++i)
                    skeletonBuilder.append('0');
                for (unsigned i = 0; i < intlInstance->m_maximumFractionDigits - intlInstance->m_minimumFractionDigits; ++i)
                    skeletonBuilder.append('#');
                skeletonBuilder.append('/');
                for (unsigned i = 0; i < intlInstance->m_minimumSignificantDigits; ++i)
                    skeletonBuilder.append('@');
                for (unsigned i = 0; i < intlInstance->m_maximumSignificantDigits - intlInstance->m_minimumSignificantDigits; ++i)
                    skeletonBuilder.append('#');
                skeletonBuilder.append(intlInstance->m_roundingType == IntlRoundingType::MorePrecision ? 'r' : 's');
            }
            break;
        }
    }
}

class IntlFieldIterator {
public:
    WTF_MAKE_NONCOPYABLE(IntlFieldIterator);

    explicit IntlFieldIterator(UFieldPositionIterator& iterator)
        : m_iterator(iterator)
    {
    }

    int32_t next(int32_t& beginIndex, int32_t& endIndex, UErrorCode&)
    {
        return ufieldpositer_next(&m_iterator, &beginIndex, &endIndex);
    }

private:
    UFieldPositionIterator& m_iterator;
};

// https://tc39.es/ecma402/#sec-unwrapnumberformat
inline IntlNumberFormat* IntlNumberFormat::unwrapForOldFunctions(JSGlobalObject* globalObject, JSValue thisValue)
{
    return unwrapForLegacyIntlConstructor<IntlNumberFormat>(globalObject, thisValue, globalObject->numberFormatConstructor());
}

// https://tc39.es/proposal-intl-numberformat-v3/out/numberformat/diff.html#sec-tointlmathematicalvalue
inline IntlMathematicalValue toIntlMathematicalValue(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto number = JSBigInt::tryExtractDouble(value))
        return IntlMathematicalValue { number.value() };

    JSValue primitive = value.toPrimitive(globalObject, PreferredPrimitiveType::PreferNumber);
    RETURN_IF_EXCEPTION(scope, { });

    auto bigIntToIntlMathematicalValue = [](JSGlobalObject* globalObject, JSValue value) -> IntlMathematicalValue {
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (auto number = JSBigInt::tryExtractDouble(value))
            return IntlMathematicalValue { number.value() };

        auto* bigInt = value.asHeapBigInt();
        auto string = bigInt->toString(globalObject, 10);
        RETURN_IF_EXCEPTION(scope, { });
        return IntlMathematicalValue {
            IntlMathematicalValue::NumberType::Integer,
            bigInt->sign(),
            string.ascii(),
        };
    };

    if (primitive.isBigInt())
        RELEASE_AND_RETURN(scope, bigIntToIntlMathematicalValue(globalObject, primitive));

    if (!primitive.isString())
        RELEASE_AND_RETURN(scope, IntlMathematicalValue { primitive.toNumber(globalObject) });

    String string = asString(primitive)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue bigInt = JSBigInt::stringToBigInt(globalObject, string);
    if (bigInt) {
        // If it is -0, we cannot handle it in JSBigInt. Reparse the string as double.
#if USE(BIGINT32)
        if (bigInt.isBigInt32() && !value.bigInt32AsInt32())
            return IntlMathematicalValue { jsToNumber(string) };
#endif
        if (bigInt.isHeapBigInt() && !asHeapBigInt(bigInt)->length())
            return IntlMathematicalValue { jsToNumber(string) };
        RELEASE_AND_RETURN(scope, bigIntToIntlMathematicalValue(globalObject, bigInt));
    }

    return IntlMathematicalValue { jsToNumber(string) };
}

} // namespace JSC
