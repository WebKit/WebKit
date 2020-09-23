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

namespace JSC  {

template<typename IntlType>
void setNumberFormatDigitOptions(JSGlobalObject* globalObject, IntlType* intlInstance, JSObject* options, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation notation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned minimumIntegerDigits = intlNumberOption(globalObject, options, vm.propertyNames->minimumIntegerDigits, 1, 21, 1);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue minimumFractionDigitsValue = options->get(globalObject, vm.propertyNames->minimumFractionDigits);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue maximumFractionDigitsValue = options->get(globalObject, vm.propertyNames->maximumFractionDigits);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue minimumSignificantDigitsValue = options->get(globalObject, vm.propertyNames->minimumSignificantDigits);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue maximumSignificantDigitsValue = options->get(globalObject, vm.propertyNames->maximumSignificantDigits);
    RETURN_IF_EXCEPTION(scope, void());

    intlInstance->m_minimumIntegerDigits = minimumIntegerDigits;

    if (!minimumSignificantDigitsValue.isUndefined() || !maximumSignificantDigitsValue.isUndefined()) {
        intlInstance->m_roundingType = IntlRoundingType::SignificantDigits;
        unsigned minimumSignificantDigits = intlDefaultNumberOption(globalObject, minimumSignificantDigitsValue, vm.propertyNames->minimumSignificantDigits, 1, 21, 1);
        RETURN_IF_EXCEPTION(scope, void());
        unsigned maximumSignificantDigits = intlDefaultNumberOption(globalObject, maximumSignificantDigitsValue, vm.propertyNames->maximumSignificantDigits, minimumSignificantDigits, 21, 21);
        RETURN_IF_EXCEPTION(scope, void());
        intlInstance->m_minimumSignificantDigits = minimumSignificantDigits;
        intlInstance->m_maximumSignificantDigits = maximumSignificantDigits;
        return;
    }

    if (!minimumFractionDigitsValue.isUndefined() || !maximumFractionDigitsValue.isUndefined()) {
        constexpr unsigned undefinedValue = UINT32_MAX;
        intlInstance->m_roundingType = IntlRoundingType::FractionDigits;
        unsigned specifiedMinimumFractionDigits = intlDefaultNumberOption(globalObject, minimumFractionDigitsValue, vm.propertyNames->minimumFractionDigits, 0, 20, undefinedValue);
        RETURN_IF_EXCEPTION(scope, void());
        unsigned specifiedMaximumFractionDigits = intlDefaultNumberOption(globalObject, maximumFractionDigitsValue, vm.propertyNames->maximumFractionDigits, 0, 20, undefinedValue);
        RETURN_IF_EXCEPTION(scope, void());
        if (specifiedMaximumFractionDigits != undefinedValue)
            minimumFractionDigitsDefault = std::min(minimumFractionDigitsDefault, specifiedMaximumFractionDigits);
        unsigned minimumFractionDigits = intlDefaultNumberOption(globalObject, (specifiedMinimumFractionDigits == undefinedValue) ? jsUndefined() : jsNumber(specifiedMinimumFractionDigits), vm.propertyNames->minimumFractionDigits, 0, 20, minimumFractionDigitsDefault);
        RETURN_IF_EXCEPTION(scope, void());
        unsigned maximumFractionDigits = intlDefaultNumberOption(globalObject, (specifiedMaximumFractionDigits == undefinedValue) ? jsUndefined() : jsNumber(specifiedMaximumFractionDigits), vm.propertyNames->maximumFractionDigits, 0, 20, std::max(maximumFractionDigitsDefault, minimumFractionDigits));
        RETURN_IF_EXCEPTION(scope, void());
        if (minimumFractionDigits > maximumFractionDigits) {
            throwRangeError(globalObject, scope, "Computed minimumFractionDigits is larger than maximumFractionDigits"_s);
            return;
        }
        intlInstance->m_minimumFractionDigits = minimumFractionDigits;
        intlInstance->m_maximumFractionDigits = maximumFractionDigits;
        return;
    }

    if (notation == IntlNotation::Compact) {
        intlInstance->m_roundingType = IntlRoundingType::CompactRounding;
        return;
    }

    intlInstance->m_roundingType = IntlRoundingType::FractionDigits;
    intlInstance->m_minimumFractionDigits = minimumFractionDigitsDefault;
    intlInstance->m_maximumFractionDigits = maximumFractionDigitsDefault;
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

} // namespace JSC
