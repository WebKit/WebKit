/*
 * Copyright (C) 2014 Igalia S.L.
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

class RenderThemeWPE final : public RenderTheme {
public:
    friend NeverDestroyed<RenderThemeWPE>;

    String extraDefaultStyleSheet() override;
#if ENABLE(VIDEO)
    String mediaControlsStyleSheet() override;
    String mediaControlsScript() override;
#endif

private:
    RenderThemeWPE() = default;
    virtual ~RenderThemeWPE() = default;

    bool supportsHover(const RenderStyle&) const override { return true; }
    bool supportsFocusRing(const RenderStyle&) const override;

    void updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription&) const override;

    Color platformFocusRingColor(OptionSet<StyleColor::Options>) const override;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const override;

    Color platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const override;
    Color platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const override;

    bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) override;

    bool popsMenuBySpaceOrReturn() const override { return true; }
    LengthBox popupInternalPaddingBox(const RenderStyle&) const override;
    bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    bool paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    Seconds animationRepeatIntervalForProgressBar(RenderProgress&) const override;
    Seconds animationDurationForProgressBar(RenderProgress&) const override;
    IntRect progressBarRectForBounds(const RenderObject&, const IntRect&) const override;
    bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) override;

    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;
    void adjustSliderThumbSize(RenderStyle&, const Element*) const override;
    bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) override;
};

} // namespace WebCore
