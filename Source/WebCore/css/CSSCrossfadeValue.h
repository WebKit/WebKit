/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

namespace WebCore {

class CSSPrimitiveValue;
class StyleImage;

namespace Style {
class BuilderState;
}

class CSSCrossfadeValue final : public CSSValue {
public:
    static Ref<CSSCrossfadeValue> create(Ref<CSSValue>&& fromValueOrNone, Ref<CSSValue>&& toValueOrNone, Ref<CSSPrimitiveValue>&& percentageValue, bool isPrefixed = false);

    ~CSSCrossfadeValue();

    bool equals(const CSSCrossfadeValue&) const;
    bool equalInputImages(const CSSCrossfadeValue&) const;

    String customCSSText() const;
    bool isPrefixed() const { return m_isPrefixed; }

    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSCrossfadeValue(Ref<CSSValue>&& fromValueOrNone, Ref<CSSValue>&& toValueOrNone, Ref<CSSPrimitiveValue>&& percentageValue, bool isPrefixed);

    Ref<CSSValue> m_fromValueOrNone;
    Ref<CSSValue> m_toValueOrNone;
    Ref<CSSPrimitiveValue> m_percentageValue;
    bool m_isPrefixed;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCrossfadeValue, isCrossfadeValue())
