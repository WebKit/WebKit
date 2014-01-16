/*
 * Copyright (C) 2012, 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderThemeNix_h
#define RenderThemeNix_h

#include "RenderTheme.h"

namespace WebCore {

class RenderThemeNix : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    virtual ~RenderThemeNix();

    virtual String extraDefaultStyleSheet() override;
    virtual String extraQuirksStyleSheet() override;
    virtual String extraPlugInsStyleSheet() override;

    virtual void systemFont(WebCore::CSSValueID, FontDescription&) const override;

#if ENABLE(PROGRESS_ELEMENT)
    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const override;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress*) const override;
#endif

#if ENABLE(METER_ELEMENT)
    virtual IntSize meterSizeForBounds(const RenderMeter*, const IntRect&) const override;
    virtual bool supportsMeter(ControlPart) const override;
#endif

protected:
    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveSelectionForegroundColor() const override;
    virtual Color platformInactiveSelectionForegroundColor() const override;

    virtual Color platformActiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformActiveListBoxSelectionForegroundColor() const override;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const override;

    // Highlighting colors for TextMatches.
    virtual Color platformActiveTextSearchHighlightColor() const override;
    virtual Color platformInactiveTextSearchHighlightColor() const override;

    virtual Color platformFocusRingColor() const override;

#if ENABLE(TOUCH_EVENTS)
    virtual Color platformTapHighlightColor() const override;
#endif

    virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintCheckbox(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void setCheckboxSize(RenderStyle*) const override;

    virtual bool paintRadio(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void setRadioSize(RenderStyle*) const override;

    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustMenuListStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintMenuListButtonDecorations(RenderObject* renderer, const PaintInfo& paintInfo, const IntRect& rect) override
    {
        return paintMenuList(renderer, paintInfo, rect);
    }

    virtual void adjustInnerSpinButtonStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintInnerSpinButton(RenderObject*, const PaintInfo&, const IntRect&) override;

#if ENABLE(PROGRESS_ELEMENT)
    virtual void adjustProgressBarStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&) override;
#endif

#if ENABLE(METER_ELEMENT)
    virtual void adjustMeterStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintMeter(RenderObject*, const PaintInfo&, const IntRect&) override;
#endif

#if ENABLE(VIDEO)
    virtual String extraMediaControlsStyleSheet() override;
    virtual bool usesVerticalVolumeSlider() const override { return false; }
    virtual bool usesMediaControlStatusDisplay() override { return true; }
    virtual bool usesMediaControlVolumeSlider() const override { return false; }

    virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSeekBackButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSeekForwardButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaRewindButton(RenderObject*, const PaintInfo&, const IntRect&) override;
#endif

    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderTrackStyle(StyleResolver*, RenderStyle*, Element*) const override;

    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustSliderThumbStyle(StyleResolver*, RenderStyle*, Element*) const override;

    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const override;

#if ENABLE(DATALIST_ELEMENT)
    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;
    virtual LayoutUnit sliderTickSnappingThreshold() const override;

    virtual bool supportsDataListUI(const AtomicString&) const override;
#endif

private:
    RenderThemeNix();
};

} // namespace WebCore

#endif // RenderThemeNix_h
