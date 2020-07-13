/*
 * Copyright (C) 2020 Igalia S.L.
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

#include "RenderTheme.h"

namespace WebCore {

class RenderThemeAdwaita : public RenderTheme {
public:
    virtual ~RenderThemeAdwaita() = default;

private:
    bool canPaint(const PaintInfo&) const final { return true; }

    String extraDefaultStyleSheet() final;
#if ENABLE(VIDEO)
    String extraMediaControlsStyleSheet() final;
    String mediaControlsScript() final;
#endif

    bool supportsHover(const RenderStyle&) const final { return true; }
    bool supportsFocusRing(const RenderStyle&) const final;
    bool shouldHaveCapsLockIndicator(const HTMLInputElement&) const final;

    void updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription&) const override { };

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const final;
    Color platformFocusRingColor(OptionSet<StyleColor::Options>) const final;
    void platformColorsDidChange() final;

    bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) final;

    bool popsMenuBySpaceOrReturn() const final { return true; }
    void adjustMenuListStyle(RenderStyle&, const Element*) const override;
    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const override;
    LengthBox popupInternalPaddingBox(const RenderStyle&) const final;
    bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    bool paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    Seconds animationRepeatIntervalForProgressBar(RenderProgress&) const final;
    Seconds animationDurationForProgressBar(RenderProgress&) const final;
    IntRect progressBarRectForBounds(const RenderObject&, const IntRect&) const final;
    bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) final;

    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void adjustSliderThumbSize(RenderStyle&, const Element*) const final;
    bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) final;

#if ENABLE(VIDEO)
    bool paintMediaSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) final;
    bool paintMediaVolumeSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) final;
#endif

#if ENABLE(DATALIST_ELEMENT)
    IntSize sliderTickSize() const final;
    int sliderTickOffsetFromTrackCenter() const final;
    void adjustListButtonStyle(RenderStyle&, const Element*) const final;
#endif
};

} // namespace WebCore
