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

#include "CSSFlexWrap.h"
#include "CSSValue.h"

namespace WebCore {

class CSSFlexWrapValue final : public CSSValue {
public:
    // Called by the generated `flex-wrap` parser with the two keywords matched by
    // `[ wrap | wrap-reverse ] || balance`, in grammar order rather than input order.
    static Ref<CSSFlexWrapValue> create(Ref<CSSValue> wrap, Ref<CSSValue> balance);

    const CSS::FlexWrap& flexWrap() const LIFETIME_BOUND { return m_flexWrap; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSFlexWrapValue&) const;

private:
    CSSFlexWrapValue(CSS::FlexWrap&&);

    CSS::FlexWrap m_flexWrap;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSFlexWrapValue, isFlexWrapValue())
