/*
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if USE(THEME_ADWAITA)

#include "RenderTheme.h"

namespace WebCore {

class RenderThemeAdwaita : public RenderTheme {
public:
    virtual ~RenderThemeAdwaita() = default;

    void setAccentColor(const Color&);

private:
    String extraDefaultStyleSheet() final;
#if ENABLE(VIDEO)
    Vector<String, 2> mediaControlsScripts() final;
    String mediaControlsStyleSheet() final;
#endif

#if ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)
    String mediaControlsBase64StringForIconNameAndType(const String&, const String&) final;
    String mediaControlsFormattedStringForDuration(double) final;

    String m_mediaControlsStyleSheet;
#endif // ENABLE(VIDEO) && ENABLE(MODERN_MEDIA_CONTROLS)

    bool supportsHover(const RenderStyle&) const final { return true; }
    bool supportsFocusRing(const RenderStyle&) const final;
    bool supportsSelectionForegroundColors(OptionSet<StyleColorOptions>) const final { return false; }
    bool supportsListBoxSelectionForegroundColors(OptionSet<StyleColorOptions>) const final { return true; }
    bool shouldHaveCapsLockIndicator(const HTMLInputElement&) const final;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformActiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformFocusRingColor(OptionSet<StyleColorOptions>) const final;
    void platformColorsDidChange() final;

    bool paintTextField(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    void adjustTextFieldStyle(RenderStyle&, const Element*) const final;

    bool paintTextArea(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    void adjustTextAreaStyle(RenderStyle&, const Element*) const final;

    bool paintSearchField(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void adjustSearchFieldStyle(RenderStyle&, const Element*) const final;

    bool popsMenuBySpaceOrReturn() const final { return true; }
    void adjustMenuListStyle(RenderStyle&, const Element*) const override;
    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const override;
    LengthBox popupInternalPaddingBox(const RenderStyle&) const final;
    bool paintMenuList(const RenderObject&, const PaintInfo&, const FloatRect&) final;
    void paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    Seconds animationRepeatIntervalForProgressBar(const RenderProgress&) const final;
    Seconds animationDurationForProgressBar() const final;
    IntRect progressBarRectForBounds(const RenderProgress&, const IntRect&) const final;
    bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) final;

    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void adjustSliderThumbSize(RenderStyle&, const Element*) const final;
    bool paintSliderThumb(const RenderObject&, const PaintInfo&, const IntRect&) final;

    Color systemColor(CSSValueID, OptionSet<StyleColorOptions>) const final;

#if ENABLE(DATALIST_ELEMENT)
    IntSize sliderTickSize() const final;
    int sliderTickOffsetFromTrackCenter() const final;
    void adjustListButtonStyle(RenderStyle&, const Element*) const final;
#endif

#if PLATFORM(GTK)
    std::optional<Seconds> caretBlinkInterval() const override;
#endif
};

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
