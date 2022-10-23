/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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

#pragma once

#include "CSSValue.h"
#include <wtf/Function.h>

namespace WebCore {

namespace Style {
class BuilderState;
}

class StyleImage;

class CSSFilterImageValue final : public CSSValue {
public:
    static Ref<CSSFilterImageValue> create(Ref<CSSValue>&& imageValueOrNone, Ref<CSSValue>&& filterValue)
    {
        return adoptRef(*new CSSFilterImageValue(WTFMove(imageValueOrNone), WTFMove(filterValue)));
    }
    ~CSSFilterImageValue();

    bool equals(const CSSFilterImageValue&) const;
    bool equalInputImages(const CSSFilterImageValue&) const;

    String customCSSText() const;

    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    explicit CSSFilterImageValue(Ref<CSSValue>&& imageValueOrNone, Ref<CSSValue>&& filterValue);

    Ref<CSSValue> m_imageValueOrNone;
    Ref<CSSValue> m_filterValue;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSFilterImageValue, isFilterImageValue())
