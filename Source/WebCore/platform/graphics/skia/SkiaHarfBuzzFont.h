/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)

#include "HbUniquePtr.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkFont.h>
#include <skia/core/SkTypeface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/RefCounted.h>

namespace WebCore {

class FontPlatformData;

class SkiaHarfBuzzFont final : public RefCounted<SkiaHarfBuzzFont> {
public:
    static Ref<SkiaHarfBuzzFont> getOrCreate(SkTypeface&);

    hb_font_t* scaledFont(const FontPlatformData&);

    std::optional<hb_codepoint_t> glyph(hb_codepoint_t, std::optional<hb_codepoint_t> variation = std::nullopt);
    hb_position_t glyphWidth(hb_codepoint_t);
    void glyphWidths(unsigned count, const hb_codepoint_t* glyphs, unsigned glyphStride, hb_position_t* advances, unsigned advanceStride);
    void glyphExtents(hb_codepoint_t, hb_glyph_extents_t*);

    ~SkiaHarfBuzzFont();

private:
    friend class SkiaHarfBuzzFontCache;

    explicit SkiaHarfBuzzFont(SkTypeface&);

    SkTypefaceID m_uniqueID { 0 };
    HbUniquePtr<hb_font_t> m_font;
    SkFont m_scaledFont;
};

} // namespace WebCore

#endif // USE(SKIA)
