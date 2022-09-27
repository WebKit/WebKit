/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSNumericValue.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSMathInvert.h"
#include "CSSMathMax.h"
#include "CSSMathMin.h"
#include "CSSMathNegate.h"
#include "CSSMathProduct.h"
#include "CSSMathSum.h"
#include "CSSNumericArray.h"
#include "CSSNumericFactory.h"
#include "CSSNumericType.h"
#include "CSSUnitValue.h"
#include "ExceptionOr.h"
#include <wtf/Algorithms.h>
#include <wtf/FixedVector.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSNumericValue);

static Ref<CSSNumericValue> negate(Ref<CSSNumericValue>&& value)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssmath-negate-a-cssnumericvalue
    if (auto* mathNegate = dynamicDowncast<CSSMathNegate>(value.get()))
        return mathNegate->value();
    if (auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get()))
        return CSSUnitValue::create(-unitValue->value(), unitValue->unitEnum());
    return CSSMathNegate::create(WTFMove(value));
}

static ExceptionOr<Ref<CSSNumericValue>> invert(Ref<CSSNumericValue>&& value)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssmath-invert-a-cssnumericvalue
    if (auto* mathInvert = dynamicDowncast<CSSMathInvert>(value.get()))
        return Ref { mathInvert->value() };

    if (auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get())) {
        if (unitValue->unitEnum() == CSSUnitType::CSS_NUMBER) {
            if (unitValue->value() == 0.0 || unitValue->value() == -0.0)
                return Exception { RangeError };
            return Ref<CSSNumericValue> { CSSUnitValue::create(1.0 / unitValue->value(), unitValue->unitEnum()) };
        }
    }

    return Ref<CSSNumericValue> { CSSMathInvert::create(WTFMove(value)) };
}

template<typename T>
static RefPtr<CSSNumericValue> operationOnValuesOfSameUnit(T&& operation, const Vector<Ref<CSSNumericValue>>& values)
{
    bool allValuesHaveSameUnit = values.size() && WTF::allOf(values, [&] (const Ref<CSSNumericValue>& value) {
        auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get());
        return unitValue ? unitValue->unitEnum() == downcast<CSSUnitValue>(values[0].get()).unitEnum() : false;
    });
    if (allValuesHaveSameUnit) {
        auto& firstUnitValue = downcast<CSSUnitValue>(values[0].get());
        auto unit = firstUnitValue.unitEnum();
        double result = firstUnitValue.value();
        for (size_t i = 1; i < values.size(); ++i)
            result = operation(result, downcast<CSSUnitValue>(values[i].get()).value());
        return CSSUnitValue::create(result, unit);
    }
    return nullptr;
}

template<typename T> Vector<Ref<CSSNumericValue>> CSSNumericValue::prependItemsOfTypeOrThis(Vector<Ref<CSSNumericValue>>&& numericValues)
{
    Vector<Ref<CSSNumericValue>> values;
    if (T* t = dynamicDowncast<T>(*this))
        values.appendVector(t->values().array());
    else
        values.append(*this);
    values.appendVector(numericValues);
    return values;
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::addInternal(Vector<Ref<CSSNumericValue>>&& numericValues)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-add
    auto values = prependItemsOfTypeOrThis<CSSMathSum>(WTFMove(numericValues));

    if (auto result = operationOnValuesOfSameUnit(std::plus<double>(), values))
        return { *result };

    auto sum = CSSMathSum::create(WTFMove(values));
    if (sum.hasException())
        return sum.releaseException();
    return Ref<CSSNumericValue> { sum.releaseReturnValue() };
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::add(FixedVector<CSSNumberish>&& values)
{
    return addInternal(WTF::map(WTFMove(values), rectifyNumberish));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::sub(FixedVector<CSSNumberish>&& values)
{
    return addInternal(WTF::map(WTFMove(values), [] (CSSNumberish&& numberish) {
        return negate(rectifyNumberish(WTFMove(numberish)));
    }));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::multiplyInternal(Vector<Ref<CSSNumericValue>>&& numericValues)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-mul
    auto values = prependItemsOfTypeOrThis<CSSMathProduct>(WTFMove(numericValues));

    bool allUnitValues = WTF::allOf(values, [&] (const Ref<CSSNumericValue>& value) {
        return is<CSSUnitValue>(value.get());
    });
    if (allUnitValues) {
        bool multipleUnitsFound { false };
        std::optional<size_t> nonNumberUnitIndex;
        for (size_t i = 0; i < values.size(); ++i) {
            auto unit = downcast<CSSUnitValue>(values[i].get()).unitEnum();
            if (unit == CSSUnitType::CSS_NUMBER)
                continue;
            if (nonNumberUnitIndex) {
                multipleUnitsFound = true;
                break;
            }
            nonNumberUnitIndex = i;
        }
        if (!multipleUnitsFound) {
            double product = 1;
            for (const Ref<CSSNumericValue>& value : values)
                product *= downcast<CSSUnitValue>(value.get()).value();
            auto unit = nonNumberUnitIndex ? downcast<CSSUnitValue>(values[*nonNumberUnitIndex].get()).unitEnum() : CSSUnitType::CSS_NUMBER;
            return { CSSUnitValue::create(product, unit) };
        }
    }

    auto product = CSSMathProduct::create(WTFMove(values));
    if (product.hasException())
        return product.releaseException();
    return Ref<CSSNumericValue> { product.releaseReturnValue() };
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::mul(FixedVector<CSSNumberish>&& values)
{
    return multiplyInternal(WTF::map(WTFMove(values), rectifyNumberish));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::div(FixedVector<CSSNumberish>&& values)
{
    Vector<Ref<CSSNumericValue>> invertedValues;
    invertedValues.reserveInitialCapacity(values.size());
    for (auto&& value : WTFMove(values)) {
        auto inverted = invert(rectifyNumberish(WTFMove(value)));
        if (inverted.hasException())
            return inverted.releaseException();
        invertedValues.uncheckedAppend(inverted.releaseReturnValue());
    }
    return multiplyInternal(WTFMove(invertedValues));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::min(FixedVector<CSSNumberish>&& numberishes)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-min
    auto values = prependItemsOfTypeOrThis<CSSMathMin>(WTF::map(WTFMove(numberishes), rectifyNumberish));
    
    if (auto result = operationOnValuesOfSameUnit<const double&(*)(const double&, const double&)>(std::min<double>, values))
        return { *result };

    auto result = CSSMathMin::create(WTFMove(values));
    if (result.hasException())
        return result.releaseException();
    return { result.releaseReturnValue() };
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::max(FixedVector<CSSNumberish>&& numberishes)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-max
    auto values = prependItemsOfTypeOrThis<CSSMathMax>(WTF::map(WTFMove(numberishes), rectifyNumberish));
    
    if (auto result = operationOnValuesOfSameUnit<const double&(*)(const double&, const double&)>(std::max<double>, values))
        return { *result };

    auto result = CSSMathMax::create(WTFMove(values));
    if (result.hasException())
        return result.releaseException();
    return { result.releaseReturnValue() };
}

Ref<CSSNumericValue> CSSNumericValue::rectifyNumberish(CSSNumberish&& numberish)
{
    // https://drafts.css-houdini.org/css-typed-om/#rectify-a-numberish-value
    return WTF::switchOn(numberish, [](RefPtr<CSSNumericValue>& value) {
        RELEASE_ASSERT(!!value);
        return Ref<CSSNumericValue> { *value };
    }, [](double value) {
        return Ref<CSSNumericValue> { CSSNumericFactory::number(value) };
    });
}

bool CSSNumericValue::equals(FixedVector<CSSNumberish>&& values)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-equals
    auto numericValues = WTF::map(WTFMove(values), rectifyNumberish);
    return WTF::allOf(numericValues, [&] (const Ref<CSSNumericValue>& value) {
        return this->equals(value.get());
    });
}

ExceptionOr<Ref<CSSUnitValue>> CSSNumericValue::to(String&& unit)
{
    return to(CSSUnitValue::parseUnit(unit));
}

ExceptionOr<Ref<CSSUnitValue>> CSSNumericValue::to(CSSUnitType unit)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-to
    auto type = CSSNumericType::create(unit);
    if (!type)
        return Exception { SyntaxError };

    auto sumValue = toSumValue();
    if (!sumValue || sumValue->size() != 1)
        return Exception { TypeError };

    auto& addend = (*sumValue)[0];
    auto unconverted = [] (const auto& addend) -> RefPtr<CSSUnitValue> {
        switch (addend.units.size()) {
        case 0:
            return CSSUnitValue::create(addend.value, CSSUnitType::CSS_NUMBER);
        case 1:
            return CSSUnitValue::create(addend.value, addend.units.begin()->key);
        default:
            break;
        }
        return nullptr;
    } (addend);
    if (!unconverted)
        return Exception { TypeError };

    auto converted = unconverted->convertTo(unit);
    if (!converted)
        return Exception { TypeError };
    return converted.releaseNonNull();
}

ExceptionOr<Ref<CSSMathSum>> CSSNumericValue::toSum(FixedVector<String>&& units)
{
    UNUSED_PARAM(units);
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-tosum
    // FIXME: add impl.
    return CSSMathSum::create(FixedVector<CSSNumberish> { 1.0 });
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::parse(String&& cssText)
{
    UNUSED_PARAM(cssText);
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-parse
    // FIXME: add impl.
    return Exception { NotSupportedError, "Not implemented Error"_s };
}

} // namespace WebCore

#endif
