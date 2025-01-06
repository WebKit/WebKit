/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveValue.h"
#include "CSSValue.h"
#include <wtf/Function.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSImageSetOptionValue final : public CSSValue {
public:
    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&);
    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&);
    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&, String);

    bool equals(const CSSImageSetOptionValue&) const;
    String customCSSText() const;

    Ref<CSSValue> image() const { return m_image; }

    Ref<CSSPrimitiveValue> resolution() const { return m_resolution; }
    void setResolution(Ref<CSSPrimitiveValue>&&);

    String type() const { return m_mimeType; }
    void setType(String);

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (func(m_image.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_resolution.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }
    bool customTraverseSubresources(const Function<bool(const CachedResource&)>&) const;
    void customSetReplacementURLForSubresources(const UncheckedKeyHashMap<String, String>&);
    void customClearReplacementURLForSubresources();

private:
    CSSImageSetOptionValue(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&);
    CSSImageSetOptionValue(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&, String&&);

    Ref<CSSValue> m_image;
    Ref<CSSPrimitiveValue> m_resolution;
    String m_mimeType;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSImageSetOptionValue, isImageSetOptionValue())
