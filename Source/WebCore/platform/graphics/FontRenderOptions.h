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

#if USE(CAIRO) || USE(SKIA)
#include <optional>
#include <wtf/NeverDestroyed.h>

#if USE(CAIRO)
#include "CairoUniquePtr.h"
#elif USE(SKIA)
#include <skia/core/SkFont.h>
#include <skia/core/SkFontTypes.h>
#include <skia/core/SkSurfaceProps.h>
#endif

namespace WebCore {

class FontRenderOptions {
    friend class NeverDestroyed<FontRenderOptions>;
public:
    WEBCORE_EXPORT static FontRenderOptions& singleton();

    enum class Hinting {
        None,
        Slight,
        Medium,
        Full
    };

    enum class Antialias {
        None,
        Normal,
        Subpixel
    };

    enum class SubpixelOrder {
        Unknown,
        HorizontalRGB,
        HorizontalBGR,
        VerticalRGB,
        VerticalBGR
    };

    void setHinting(std::optional<Hinting>);
    void setAntialias(std::optional<Antialias>);
    void setSubpixelOrder(std::optional<SubpixelOrder>);

#if USE(CAIRO)
    const cairo_font_options_t* fontOptions() const { return m_fontOptions.get(); }
#elif USE(SKIA)
    SkFontHinting hinting() const { return m_hinting; }
    SkFont::Edging antialias() const { return m_antialias; }
    SkPixelGeometry subpixelOrder() const { return m_subpixelOrder; }
#endif

    WEBCORE_EXPORT void disableHintingForTesting();

private:
    FontRenderOptions();
    ~FontRenderOptions() = default;

#if USE(CAIRO)
    CairoUniquePtr<cairo_font_options_t> m_fontOptions;
#elif USE(SKIA)
    SkFontHinting m_hinting { SkFontHinting::kNormal };
    SkFont::Edging m_antialias { SkFont::Edging::kAntiAlias };
    SkPixelGeometry m_subpixelOrder { kUnknown_SkPixelGeometry };
#endif
    bool m_isHintingDisabledForTesting { false };
};

} // namespace WebCore

#endif // USE(CAIRO) || USE(SKIA)
