/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSValue.h"
#include "Length.h"

namespace WebCore {

class Element;

class CSSAnchorValue final : public CSSValue {
public:
    void collectAnchorNames(HashSet<String>& accumulator) const;

    static Ref<CSSAnchorValue> create(RefPtr<CSSPrimitiveValue>&& anchorElement, Ref<CSSValue>&& anchorSide, RefPtr<CSSPrimitiveValue>&& fallback);

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    String customCSSText() const;
    bool equals(const CSSAnchorValue&) const;

    String anchorElementString() const;
    Ref<CSSValue> anchorSide() const;

private:
    CSSAnchorValue(RefPtr<CSSPrimitiveValue>&& anchorElement, Ref<CSSValue>&& anchorSide, RefPtr<CSSPrimitiveValue>&& fallback)
        : CSSValue(AnchorClass)
        , m_anchorElement(WTFMove(anchorElement))
        , m_anchorSide(WTFMove(anchorSide))
        , m_fallback(WTFMove(fallback))
    {
    }

    RefPtr<CSSPrimitiveValue> m_anchorElement;
    Ref<CSSValue> m_anchorSide;
    RefPtr<CSSPrimitiveValue> m_fallback;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSAnchorValue, isAnchorValue())
