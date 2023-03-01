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

#include "CSSValue.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSPrimitiveValue;

class CSSImageSetOptionValue final : public CSSValue {
public:
    class Type {
    public:
        Type(String);

        bool isSupported() const { return m_isSupported; }
        String mimeType() const { return m_mimeType; }
        String cssText() const;

    private:
        bool m_isSupported;
        String m_mimeType;
    };

    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&);
    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&);
    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&, String);

    bool equals(const CSSImageSetOptionValue&) const;
    String customCSSText() const;

    Ref<CSSValue> image() const;

    RefPtr<CSSPrimitiveValue> resolution() const;
    void setResolution(Ref<CSSPrimitiveValue>&&);

    std::optional<Type> type() const;
    void setType(Type&&);
    void setType(String&&);

private:
    CSSImageSetOptionValue(Ref<CSSValue>&&);
    CSSImageSetOptionValue(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&);
    CSSImageSetOptionValue(Ref<CSSValue>&&, Ref<CSSPrimitiveValue>&&, String&&);

    Ref<CSSValue> m_image;
    RefPtr<CSSPrimitiveValue> m_resolution;
    std::optional<Type> m_type;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSImageSetOptionValue, isImageSetOptionValue())
