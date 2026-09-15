/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCalcSizeValue.h"

#include "CSSPrimitiveNumericRange.h"
#include "StyleCalcSizeValue+Evaluation.h"
#include "StyleCalcSizeValue+Serialization.h"
#include "StyleCalculationTree+Conversion.h"
#include "StyleCalculationTree+Copy.h"
#include "StyleCalculationTree+Evaluation.h"
#include <cmath>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

Ref<CalcSizeValue> CalcSizeValue::create(Basis&& basis, Calculation::Tree&& calculation, bool basisHasPercentage, bool hasPercentage)
{
    return adoptRef(*new CalcSizeValue(WTF::move(basis), WTF::move(calculation), basisHasPercentage, hasPercentage));
}

CalcSizeValue::CalcSizeValue(Basis&& basis, Calculation::Tree&& calculation, bool basisHasPercentage, bool hasPercentage)
    : m_basis(WTF::move(basis))
    , m_calculation(WTF::move(calculation))
    , m_basisHasPercentage(basisHasPercentage)
    , m_hasPercentage(hasPercentage)
{
}

CalcSizeValue::~CalcSizeValue() = default;

CSSValueID CalcSizeValue::basisKeyword() const
{
    return WTF::switchOn(m_basis,
        // `any` states that the value does not depend on a basis, so it behaves as a length rather
        // than as a keyword.
        [](const CSS::Keyword::Any&) { return CSSValueInvalid; },
        [](const Calculation::Tree&) { return CSSValueInvalid; },
        [](const Ref<CalcSizeValue>& nested) { return nested->basisKeyword(); },
        []<CSSValueID Id>(const Constant<Id>&) { return Id; }
    );
}

Ref<CalcSizeValue> CalcSizeValue::copy() const
{
    auto basis = WTF::switchOn(m_basis,
        [](const Calculation::Tree& tree) -> Basis { return Calculation::copy(tree); },
        [](const Ref<CalcSizeValue>& nested) -> Basis { return nested->copy(); },
        [](const auto& keyword) -> Basis { return keyword; }
    );

    return create(WTF::move(basis), Calculation::copy(m_calculation), m_basisHasPercentage, m_hasPercentage);
}

bool CalcSizeValue::operator==(const CalcSizeValue& other) const
{
    if (m_calculation != other.m_calculation)
        return false;

    if (m_basis.index() != other.m_basis.index())
        return false;

    return WTF::switchOn(m_basis,
        [&](const Ref<CalcSizeValue>& nested) {
            return nested.get() == std::get<Ref<CalcSizeValue>>(other.m_basis).get();
        },
        [&](const auto& value) {
            return value == std::get<std::decay_t<decltype(value)>>(other.m_basis);
        }
    );
}

CSS::CalcSizeFunction toCSSCalcSizeFunction(const CalcSizeValue& value)
{
    auto options = Calculation::ToCSSOptions {
        .category = CSS::Category::LengthPercentage,
        .range = CSS::All
    };

    auto basis = WTF::switchOn(value.basis(),
        [&](const Calculation::Tree& tree) -> CSS::CalcSizeBasis {
            return Calculation::toCSS(tree, options);
        },
        [&](const Ref<CalcSizeValue>& nested) -> CSS::CalcSizeBasis {
            return makeUniqueRef<CSS::CalcSizeFunction>(toCSSCalcSizeFunction(nested.get()));
        },
        [&](const auto& keyword) -> CSS::CalcSizeBasis {
            return keyword;
        }
    );

    return CSS::CalcSizeFunction { CSS::CalcSizeParameters { WTF::move(basis), Calculation::toCSS(value.calculation(), options) } };
}

double evaluateCalcSize(const CalcSizeValue& value, double percentResolutionLength, ZoomFactor usedZoom)
{
    auto basis = WTF::switchOn(value.basis(),
        [&](const Calculation::Tree& tree) -> double {
            return Calculation::evaluate(tree, {
                .percentResolutionLength = percentResolutionLength,
                .usedZoom = usedZoom
            });
        },
        [&](const Ref<CalcSizeValue>& nested) -> double {
            return evaluateCalcSize(nested.get(), percentResolutionLength, usedZoom);
        },
        [&](const CSS::Keyword::Any&) -> double {
            // `size` is invalid with an `any` basis, so this is never read.
            return 0;
        },
        [&](const auto&) -> double {
            ASSERT_NOT_REACHED("A keyword basis is resolved by layout, not here");
            return 0;
        }
    );

    return Calculation::evaluate(value.calculation(), {
        .percentResolutionLength = percentResolutionLength,
        .usedZoom = usedZoom,
        .sizeResolutionLength = basis
    });
}

double evaluateCalcSize(const CalcSizeValue& value, CSS::Range range, double percentResolutionLength, ZoomFactor usedZoom)
{
    auto result = evaluateCalcSize(value, percentResolutionLength, usedZoom);
    if (std::isnan(result))
        return 0;
    return CSS::clampToRange<double>(result, range);
}

TextStream& operator<<(TextStream& ts, const CalcSizeValue& value)
{
    ts << "calc-size("_s;
    WTF::switchOn(value.basis(),
        [&](const Calculation::Tree& tree) { ts << tree; },
        [&](const Ref<CalcSizeValue>& nested) { ts << nested.get(); },
        [&](const auto& keyword) { ts << nameLiteralForSerialization(keyword.value); }
    );
    return ts << ", "_s << value.calculation() << ')';
}

} // namespace Style
} // namespace WebCore
