/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveNumeric.h"
#include "CSSString.h"
#include "CSSValue.h"
#include <wtf/Function.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// <image-set-option> = [ <image> | <string> ] [ <resolution> || type(<string>) ]?
// https://drafts.csswg.org/css-images-4/#typedef-image-set-option
class CSSImageSetOptionValue final : public CSSValue {
public:
    static Ref<CSSImageSetOptionValue> create(Ref<CSSValue>&&, std::optional<CSS::Resolution<>>&&, std::optional<FunctionNotation<CSSValueType, CSS::String>>&&);

    bool equals(const CSSImageSetOptionValue&) const;
    String customCSSText(const CSS::SerializationContext&) const;

    const CSSValue& image() const LIFETIME_BOUND { return m_image; }
    const CSS::Resolution<>& resolution() const LIFETIME_BOUND { return m_resolution; }
    const std::optional<FunctionNotation<CSSValueType, CSS::String>>& type() const LIFETIME_BOUND { return m_mimeType; }

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;
    bool customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>&) const;

private:
    CSSImageSetOptionValue(Ref<CSSValue>&&, std::optional<CSS::Resolution<>>&&, std::optional<FunctionNotation<CSSValueType, CSS::String>>&&);

    const Ref<CSSValue> m_image;
    const CSS::Resolution<> m_resolution;
    const std::optional<FunctionNotation<CSSValueType, CSS::String>> m_mimeType;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSImageSetOptionValue, isImageSetOptionValue())
