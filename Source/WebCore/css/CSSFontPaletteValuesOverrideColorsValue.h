/*
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

#include "CSSPrimitiveValue.h"
#include "CSSValue.h"
#include "CachedResourceHandle.h"
#include "ResourceLoaderOptions.h"
#include <wtf/Function.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedFont;
class FontLoadRequest;
class SVGFontFaceElement;
class ScriptExecutionContext;

class CSSFontPaletteValuesOverrideColorsValue final : public CSSValue {
public:
    static Ref<CSSFontPaletteValuesOverrideColorsValue> create(Ref<CSSPrimitiveValue>&& key, Ref<CSSValue>&& color)
    {
        return adoptRef(*new CSSFontPaletteValuesOverrideColorsValue(WTFMove(key), WTFMove(color)));
    }

    const CSSPrimitiveValue& key() const { return m_key; }
    const CSSValue& color() const { return m_color; }

    String customCSSText() const;

    bool equals(const CSSFontPaletteValuesOverrideColorsValue&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (func(m_key.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_color.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }

private:
    CSSFontPaletteValuesOverrideColorsValue(Ref<CSSPrimitiveValue>&& key, Ref<CSSValue>&& color)
        : CSSValue(ClassType::FontPaletteValuesOverrideColors)
        , m_key(WTFMove(key))
        , m_color(WTFMove(color))
    {
    }

    Ref<CSSPrimitiveValue> m_key;
    Ref<CSSValue> m_color;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSFontPaletteValuesOverrideColorsValue, isFontPaletteValuesOverrideColorsValue())
