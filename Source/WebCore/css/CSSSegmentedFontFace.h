/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "CSSFontFace.h"
#include "FontCache.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSFontSelector;
class FontCreationContext;
class FontDescription;
class FontPaletteValues;
class FontRanges;

class CSSSegmentedFontFace final : public RefCounted<CSSSegmentedFontFace>, public CSSFontFace::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CSSSegmentedFontFace> create()
    {
        return adoptRef(*new CSSSegmentedFontFace());
    }
    ~CSSSegmentedFontFace();

    void appendFontFace(Ref<CSSFontFace>&&);

    FontRanges fontRanges(const FontDescription&, const FontPaletteValues&, RefPtr<FontFeatureValues>);

    Vector<Ref<CSSFontFace>, 1>& constituentFaces() { return m_fontFaces; }

    // CSSFontFace::Client needs to be able to be held in a RefPtr.
    void ref() final { RefCounted<CSSSegmentedFontFace>::ref(); }
    void deref() final { RefCounted<CSSSegmentedFontFace>::deref(); }

private:
    CSSSegmentedFontFace();
    void fontLoaded(CSSFontFace&) final;

    // FIXME: Add support for font-feature-values in the key for this cache.
    HashMap<std::tuple<FontDescriptionKey, FontPaletteValues>, FontRanges> m_cache;
    Vector<Ref<CSSFontFace>, 1> m_fontFaces;
};

} // namespace WebCore
