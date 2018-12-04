/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_PAINTING_API)

#include "CSSImageValue.h"
#include "ImageBitmap.h"
#include "RenderElement.h"
#include "TypedOMCSSStyleValue.h"
#include <wtf/RefCounted.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TypedOMCSSImageValue final : public TypedOMCSSStyleValue {
public:
    static Ref<TypedOMCSSImageValue> create(CSSImageValue& cssValue, RenderElement& renderer)
    {
        return adoptRef(*new TypedOMCSSImageValue(cssValue, renderer));
    }

    String toString() final { return m_cssValue->cssText(); }

    CachedImage* image() { return m_cssValue->cachedImage(); }
    const RenderElement* renderer() const { return m_renderer.get(); }

private:
    TypedOMCSSImageValue(CSSImageValue& cssValue, RenderElement& renderer)
        : m_cssValue(makeRef(cssValue))
        , m_renderer(makeWeakPtr(renderer))
    {
    }

    bool isImageValue() final { return true; }

    Ref<CSSImageValue> m_cssValue;
    WeakPtr<RenderElement> m_renderer;
};

} // namespace WebCore

#endif
