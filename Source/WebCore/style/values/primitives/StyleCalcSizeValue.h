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

#pragma once

#include <WebCore/CSSValueTypes.h>
#include <WebCore/StyleCalculationTree.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

// calc-size() as it appears in a computed value. The function survives to used value time because
// that is when `size` resolves, and because the basis decides how the value behaves for everything
// other than resolving the size.
class CalcSizeValue : public RefCounted<CalcSizeValue> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(CalcSizeValue);
public:
    // The `-webkit-min-content` family normalizes to the unprefixed keyword during conversion, so
    // only the canonical spellings appear here.
    using Basis = Variant<
        CSS::Keyword::Any,
        CSS::Keyword::Auto,
        CSS::Keyword::Content,
        CSS::Keyword::MinContent,
        CSS::Keyword::MaxContent,
        CSS::Keyword::FitContent,
        CSS::Keyword::Stretch,
        CSS::Keyword::WebkitFillAvailable,
        CSS::Keyword::Intrinsic,
        CSS::Keyword::MinIntrinsic,
        Calculation::Tree,
        Ref<CalcSizeValue>
    >;

    WEBCORE_EXPORT static Ref<CalcSizeValue> create(Basis&&, Calculation::Tree&&, bool basisHasPercentage, bool hasPercentage);
    WEBCORE_EXPORT ~CalcSizeValue();

    const Basis& basis() const LIFETIME_BOUND { return m_basis; }

    // A percentage in the basis resolves against the containing block as normal, so when that is
    // indefinite the whole function behaves as the basis does rather than resolving against zero.
    bool basisHasPercentage() const { return m_basisHasPercentage; }

    // True if a percentage appears anywhere, including in a nested function's calculation.
    bool NODELETE hasPercentage() const { return m_hasPercentage; }
    const Calculation::Tree& calculation() const LIFETIME_BOUND { return m_calculation; }

    // The keyword the value behaves as, or CSSValueInvalid for a <calc-sum> or `any` basis, which
    // behave as an ordinary length.
    WEBCORE_EXPORT CSSValueID NODELETE basisKeyword() const;

    WEBCORE_EXPORT Ref<CalcSizeValue> copy() const;

    WEBCORE_EXPORT bool operator==(const CalcSizeValue&) const;

private:
    CalcSizeValue(Basis&&, Calculation::Tree&&, bool basisHasPercentage, bool hasPercentage);

    Basis m_basis;
    Calculation::Tree m_calculation;
    bool m_basisHasPercentage;
    bool m_hasPercentage;
};

WTF::TextStream& operator<<(WTF::TextStream&, const CalcSizeValue&);

} // namespace Style
} // namespace WebCore
