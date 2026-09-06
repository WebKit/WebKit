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
#include "CSSCalcSizeFunction.h"

#include "CSSCalcTree+ComputedStyleDependencies.h"
#include "CSSCalcTree+Serialization.h"
#include "CSSCalcTree.h"
#include "CSSSerializationContext.h"
#include "CSSValueKeywords.h"
#include <wtf/PointerComparison.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(CalcSize);

CalcSize::CalcSize(CalcSizeBasis&& basis, Ref<CSSCalc::Value>&& calculation)
    : basis(WTF::move(basis))
    , calculation(WTF::move(calculation))
{
}

static CalcSizeBasis copyBasis(const CalcSizeBasis& basis)
{
    return WTF::switchOn(basis,
        [](CSSValueID keyword) { return CalcSizeBasis { keyword }; },
        [](const Ref<CSSCalc::Value>& calculation) { return CalcSizeBasis { calculation }; },
        [](const UniqueRef<CalcSize>& nested) { return CalcSizeBasis { makeUniqueRef<CalcSize>(nested.get()) }; }
    );
}

CalcSize::CalcSize(const CalcSize& other)
    : basis(copyBasis(other.basis))
    , calculation(other.calculation)
{
}

CalcSize& CalcSize::operator=(const CalcSize& other)
{
    basis = copyBasis(other.basis);
    calculation = other.calculation;
    return *this;
}

CalcSize::CalcSize(CalcSize&&) = default;
CalcSize& CalcSize::operator=(CalcSize&&) = default;
CalcSize::~CalcSize() = default;

CSSValueID CalcSize::basisKeyword() const
{
    return WTF::switchOn(basis,
        [](CSSValueID keyword) { return keyword; },
        [](const Ref<CSSCalc::Value>&) { return CSSValueInvalid; },
        [](const UniqueRef<CalcSize>& nested) { return nested->basisKeyword(); }
    );
}

void CalcSize::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    WTF::switchOn(basis,
        [&](CSSValueID) { },
        [&](const Ref<CSSCalc::Value>& basis) { CSSCalc::collectComputedStyleDependencies(basis->tree(), dependencies); },
        [&](const UniqueRef<CalcSize>& nested) { nested->collectComputedStyleDependencies(dependencies); }
    );

    CSSCalc::collectComputedStyleDependencies(calculation->tree(), dependencies);
}

static bool calculationsEqual(const CSSCalc::Value& a, const CSSCalc::Value& b)
{
    return a.tree().root == b.tree().root;
}

bool CalcSize::operator==(const CalcSize& other) const
{
    auto basesEqual = WTF::switchOn(basis,
        [&](CSSValueID keyword) {
            auto* otherKeyword = std::get_if<CSSValueID>(&other.basis);
            return otherKeyword && *otherKeyword == keyword;
        },
        [&](const Ref<CSSCalc::Value>& basis) {
            auto* otherBasis = std::get_if<Ref<CSSCalc::Value>>(&other.basis);
            return otherBasis && calculationsEqual(basis, *otherBasis);
        },
        [&](const UniqueRef<CalcSize>& nested) {
            auto* otherNested = std::get_if<UniqueRef<CalcSize>>(&other.basis);
            return otherNested && arePointingToEqualData(nested, *otherNested);
        }
    );

    return basesEqual && calculationsEqual(calculation, other.calculation);
}

// A `<calc-sum>` in function-argument position serializes without an enclosing `calc()` and without
// grouping parentheses, per serialize-a-math-function step 4.
static void serializeCalcSum(StringBuilder& builder, const CSSCalc::Value& value, const SerializationContext& context)
{
    CSSCalc::serializationForCSSAsFunctionArgument(builder, value.tree().root, { value.range(), context });
}

void Serialize<CalcSize>::operator()(StringBuilder& builder, const SerializationContext& context, const CalcSize& value)
{
    builder.append(nameLiteralForSerialization(CSSValueCalcSize));
    builder.append('(');

    WTF::switchOn(value.basis,
        [&](CSSValueID keyword) { builder.append(nameLiteralForSerialization(keyword)); },
        [&](const Ref<CSSCalc::Value>& basis) { serializeCalcSum(builder, basis, context); },
        [&](const UniqueRef<CalcSize>& nested) { serializationForCSS(builder, context, nested.get()); }
    );

    builder.append(", "_s);
    serializeCalcSum(builder, value.calculation, context);
    builder.append(')');
}

} // namespace CSS
} // namespace WebCore
