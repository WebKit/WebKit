/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveValue.h"
#include "RenderStyleConstants.h"

namespace WebCore {

// Class containing the value of a scroll() function, as used in animation-timeline:
// https://drafts.csswg.org/scroll-animations-1/#funcdef-scroll.
class CSSScrollValue final : public CSSValue {
public:
    static Ref<CSSScrollValue> create()
    {
        return adoptRef(*new CSSScrollValue(nullptr, nullptr));
    }

    static Ref<CSSScrollValue> create(RefPtr<CSSValue>&& scroller, RefPtr<CSSValue>&& axis)
    {
        return adoptRef(*new CSSScrollValue(WTFMove(scroller), WTFMove(axis)));
    }

    String customCSSText() const;

    RefPtr<CSSValue> scroller() const { return m_scroller; }
    RefPtr<CSSValue> axis() const { return m_axis; }

    bool equals(const CSSScrollValue&) const;

private:
    CSSScrollValue(RefPtr<CSSValue>&& scroller, RefPtr<CSSValue>&& axis)
        : CSSValue(ScrollClass)
        , m_scroller(WTFMove(scroller))
        , m_axis(WTFMove(axis))
    {
    }

    RefPtr<CSSValue> m_scroller;
    RefPtr<CSSValue> m_axis;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSScrollValue, isScrollValue())
