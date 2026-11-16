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

#if USE(CAIRO)

namespace WebCore {

FontRenderOptions::FontRenderOptions()
    : m_fontOptions(cairo_font_options_create())
{
}

void FontRenderOptions::setHinting(std::optional<Hinting> hinting)
{
    if (m_isHintingDisabledForTesting)
        return;

    cairo_font_options_set_hint_metrics(m_fontOptions.get(), CAIRO_HINT_METRICS_ON);
    if (!hinting.has_value()) {
        cairo_font_options_set_hint_style(m_fontOptions.get(), CAIRO_HINT_STYLE_DEFAULT);
        return;
    }

    switch (hinting.value()) {
    case Hinting::None:
        cairo_font_options_set_hint_style(m_fontOptions.get(), CAIRO_HINT_STYLE_NONE);
        break;
    case Hinting::Slight:
        cairo_font_options_set_hint_style(m_fontOptions.get(), CAIRO_HINT_STYLE_SLIGHT);
        break;
    case Hinting::Medium:
        cairo_font_options_set_hint_style(m_fontOptions.get(), CAIRO_HINT_STYLE_MEDIUM);
        break;
    case Hinting::Full:
        cairo_font_options_set_hint_style(m_fontOptions.get(), CAIRO_HINT_STYLE_FULL);
        break;
    }
}

void FontRenderOptions::setAntialias(std::optional<Antialias> antialias)
{
    if (!antialias.has_value()) {
        cairo_font_options_set_antialias(m_fontOptions.get(), CAIRO_ANTIALIAS_DEFAULT);
        return;
    }

    switch (antialias.value()) {
    case Antialias::None:
        cairo_font_options_set_antialias(m_fontOptions.get(), CAIRO_ANTIALIAS_NONE);
        break;
    case Antialias::Normal:
        cairo_font_options_set_antialias(m_fontOptions.get(), CAIRO_ANTIALIAS_GRAY);
        break;
    case Antialias::Subpixel:
        cairo_font_options_set_antialias(m_fontOptions.get(), CAIRO_ANTIALIAS_SUBPIXEL);
        break;
    }
}

void FontRenderOptions::setSubpixelOrder(std::optional<SubpixelOrder> subpixelOrder)
{
    if (!subpixelOrder.has_value()) {
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), CAIRO_SUBPIXEL_ORDER_DEFAULT);
        return;
    }

    switch (subpixelOrder.value()) {
    case SubpixelOrder::Unknown:
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), CAIRO_SUBPIXEL_ORDER_DEFAULT);
        break;
    case SubpixelOrder::HorizontalRGB:
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), CAIRO_SUBPIXEL_ORDER_RGB);
        break;
    case SubpixelOrder::HorizontalBGR:
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), CAIRO_SUBPIXEL_ORDER_BGR);
        break;
    case SubpixelOrder::VerticalRGB:
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), CAIRO_SUBPIXEL_ORDER_VRGB);
        break;
    case SubpixelOrder::VerticalBGR:
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), CAIRO_SUBPIXEL_ORDER_VBGR);
        break;
    }
}

void FontRenderOptions::disableHintingForTesting()
{
    cairo_font_options_set_hint_metrics(m_fontOptions.get(), CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_hint_style(m_fontOptions.get(), CAIRO_HINT_STYLE_NONE);
    m_isHintingDisabledForTesting = true;
}

} // namespace WebCore

#endif // USE(CAIRO)
