/*
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
    virtual ~RenderThemeAdwaita();

    bool canCreateControlPartForRenderer(const RenderElement&) const final;
    bool canCreateControlPartForBorderOnly(const RenderElement&) const final;
    bool canCreateControlPartForDecorations(const RenderElement&) const final;

    bool isControlStyled(const RenderStyle&) const final;

    void setAccentColor(const Color&);

    String extraDefaultStyleSheet() final;
#if ENABLE(VIDEO)
    Vector<String, 2> mediaControlsScripts() final;
    Vector<String, 2> mediaControlsStyleSheets(const HTMLMediaElement&) final;
#endif

#if ENABLE(VIDEO)
    RefPtr<FragmentedSharedBuffer> mediaControlsImageDataForIconNameAndType(const String&, const String&) final;
    String mediaControlsBase64StringForIconNameAndType(const String&, const String&) final;
    String mediaControlsFormattedStringForDuration(double) final;
#endif // ENABLE(VIDEO)

    bool supportsHover() const final { return true; }
    bool supportsFocusRing(const RenderElement&, const RenderStyle&) const final;
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

    void adjustTextFieldStyle(RenderStyle&, const Element*) const final;
    void adjustTextAreaStyle(RenderStyle&, const Element*) const final;
    void adjustSearchFieldStyle(RenderStyle&, const Element*) const final;

    bool popsMenuBySpaceOrReturn() const final { return true; }
    void adjustMenuListStyle(RenderStyle&, const Element*) const final;
    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const final;
    Style::PaddingBox popupInternalPaddingBox(const RenderStyle&) const final;

    Seconds animationRepeatIntervalForProgressBar(const RenderProgress&) const final;
    IntRect progressBarRectForBounds(const RenderProgress&, const IntRect&) const final;

    void adjustSliderThumbSize(RenderStyle&, const Element*) const final;

    WEBCORE_EXPORT Color systemColor(CSSValueID, OptionSet<StyleColorOptions>) const final;

    IntSize sliderTickSize() const final;
    int sliderTickOffsetFromTrackCenter() const final;
    void adjustListButtonStyle(RenderStyle&, const Element*) const final;

    Style::PreferredSizePair controlSize(StyleAppearance, const FontCascade&, const Style::PreferredSizePair&, float) const final;
    Style::MinimumSizePair minimumControlSize(StyleAppearance, const FontCascade&, const Style::MinimumSizePair&, float zoomFactor) const final;
    Style::LineWidthBox controlBorder(StyleAppearance, const FontCascade&, const Style::LineWidthBox&, float, const Element*) const final;

#if PLATFORM(GTK) || PLATFORM(WPE)
    std::optional<Seconds> caretBlinkInterval() const final;
#endif

private:
#if ENABLE(VIDEO)
    String m_mediaControlsStyleSheet;
#endif
};

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
