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

#include <WebCore/CSSCalcSizeFunction.h>
#include <WebCore/CSSValue.h>

namespace WebCore {

// Holds a `CSS::CalcSize` so that it can live in a declaration alongside the keyword and
// <length-percentage> values the sizing properties also accept.
class CSSCalcSizeValue final : public CSSValue {
public:
    static Ref<CSSCalcSizeValue> create(CSS::CalcSize&&);

    const CSS::CalcSize& calcSize() const LIFETIME_BOUND { return m_calcSize; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSCalcSizeValue&) const;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

private:
    explicit CSSCalcSizeValue(CSS::CalcSize&&);

    CSS::CalcSize m_calcSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCalcSizeValue, isCalcSizeValue())
