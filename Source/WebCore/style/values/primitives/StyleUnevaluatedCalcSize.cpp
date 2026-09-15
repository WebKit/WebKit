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
#include "StyleUnevaluatedCalcSize.h"

#include "StyleCalcSizeValue+Evaluation.h"
#include "StyleCalcSizeValue.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

UnevaluatedCalcSize::UnevaluatedCalcSize(CalcSizeValue& value)
    : m_calcSize { value }
{
}

UnevaluatedCalcSize::UnevaluatedCalcSize(Ref<CalcSizeValue>&& value)
    : m_calcSize { WTF::move(value) }
{
}

UnevaluatedCalcSize::UnevaluatedCalcSize(const UnevaluatedCalcSize&) = default;
UnevaluatedCalcSize::UnevaluatedCalcSize(UnevaluatedCalcSize&&) = default;
UnevaluatedCalcSize& UnevaluatedCalcSize::operator=(const UnevaluatedCalcSize&) = default;
UnevaluatedCalcSize& UnevaluatedCalcSize::operator=(UnevaluatedCalcSize&&) = default;

UnevaluatedCalcSize::~UnevaluatedCalcSize() = default;

Ref<CalcSizeValue> CLANG_POINTER_CONVERSION protect(CalcSizeValue& value)
{
    return value;
}

CalcSizeValue& UnevaluatedCalcSize::leakRef()
{
    return m_calcSize.leakRef();
}

CSSValueID UnevaluatedCalcSize::basisKeyword() const
{
    return protect(m_calcSize)->basisKeyword();
}

bool UnevaluatedCalcSize::hasPercentage() const
{
    return protect(m_calcSize)->hasPercentage();
}

bool UnevaluatedCalcSize::basisHasPercentage() const
{
    return protect(m_calcSize)->basisHasPercentage();
}

double UnevaluatedCalcSize::evaluate(double percentResolutionLength, const ZoomFactor& zoom) const
{
    return evaluateCalcSize(m_calcSize, range, percentResolutionLength, zoom);
}

bool UnevaluatedCalcSize::operator==(const UnevaluatedCalcSize& other) const
{
    return protect(m_calcSize)->operator==(other.calcSize());
}

TextStream& operator<<(TextStream& ts, const UnevaluatedCalcSize& value)
{
    return ts << protect(value.calcSize());
}

CSSValueID calcSizeBasisKeyword(const CalcSizeValue& value)
{
    return value.basisKeyword();
}

bool calcSizeHasPercentage(const CalcSizeValue& value)
{
    return value.hasPercentage();
}

} // namespace Style
} // namespace WebCore
