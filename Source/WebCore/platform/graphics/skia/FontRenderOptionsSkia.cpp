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

#include "config.h"
#include "FontRenderOptions.h"

#if USE(SKIA)

namespace WebCore {

FontRenderOptions::FontRenderOptions() = default;

void FontRenderOptions::setHinting(std::optional<Hinting> hinting)
{
    if (m_isHintingDisabledForTesting)
        return;

    switch (hinting.value_or(Hinting::Medium)) {
    case Hinting::None:
        m_hinting = SkFontHinting::kNone;
        break;
    case Hinting::Slight:
        m_hinting = SkFontHinting::kSlight;
        break;
    case Hinting::Medium:
        m_hinting = SkFontHinting::kNormal;
        break;
    case Hinting::Full:
        m_hinting = SkFontHinting::kFull;
        break;
    }
}

void FontRenderOptions::setAntialias(std::optional<Antialias> antialias)
{
    switch (antialias.value_or(Antialias::Normal)) {
    case Antialias::None:
        m_antialias = SkFont::Edging::kAlias;
        break;
    case Antialias::Normal:
        m_antialias = SkFont::Edging::kAntiAlias;
        break;
    case Antialias::Subpixel:
        m_antialias = SkFont::Edging::kSubpixelAntiAlias;
        break;
    }
}

void FontRenderOptions::setSubpixelOrder(std::optional<SubpixelOrder> subpixelOrder)
{
    switch (subpixelOrder.value_or(SubpixelOrder::Unknown)) {
    case SubpixelOrder::Unknown:
        m_subpixelOrder = kUnknown_SkPixelGeometry;
        break;
    case SubpixelOrder::HorizontalRGB:
        m_subpixelOrder = kRGB_H_SkPixelGeometry;
        break;
    case SubpixelOrder::HorizontalBGR:
        m_subpixelOrder = kBGR_H_SkPixelGeometry;
        break;
    case SubpixelOrder::VerticalRGB:
        m_subpixelOrder = kRGB_V_SkPixelGeometry;
        break;
    case SubpixelOrder::VerticalBGR:
        m_subpixelOrder = kBGR_V_SkPixelGeometry;
        break;
    }
}

void FontRenderOptions::disableHintingForTesting()
{
    m_hinting = SkFontHinting::kNone;
    m_isHintingDisabledForTesting = true;
}

} // namespace WebCore

#endif // USE(SKIA)
