/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef RenderThemeIOS_h
#define RenderThemeIOS_h

#if PLATFORM(IOS)

#include "RenderTheme.h"

namespace WebCore {
    
class RenderStyle;
class GraphicsContext;
    
class RenderThemeIOS : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    virtual int popupInternalPaddingRight(RenderStyle*) const override;

    static void adjustRoundBorderRadius(RenderStyle&, RenderBox*);

    virtual void systemFont(CSSValueID, FontDescription&) const override;

    static CFStringRef contentSizeCategory();

protected:
    virtual int baselinePosition(const RenderObject*) const override;

    virtual bool isControlStyled(const RenderStyle*, const BorderData&, const FillLayer& background, const Color& backgroundColor) const override;

    // Methods for each appearance value.
    virtual void adjustCheckboxStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintCheckboxDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustRadioStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintRadioDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustButtonStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintButtonDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintPushButtonDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void setButtonSize(RenderStyle*) const override;

    virtual bool paintFileUploadIconDecorations(RenderObject* inputRenderer, RenderObject* buttonRenderer, const PaintInfo&, const IntRect&, Icon*, FileUploadDecorations) override;

    virtual bool paintTextFieldDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintTextAreaDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustMenuListButtonStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintMenuListButtonDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustSliderTrackStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const override;
    virtual bool paintSliderThumbDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;

#if ENABLE(PROGRESS_ELEMENT)
    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const override;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress*) const override;

    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&) override;
#endif

#if ENABLE(DATALIST_ELEMENT)
    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;
#endif

    virtual void adjustSearchFieldStyle(StyleResolver*, RenderStyle*, Element*) const override;
    virtual bool paintSearchFieldDecorations(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;

#if ENABLE(TOUCH_EVENTS)
    virtual Color platformTapHighlightColor() const override { return 0x4D1A1A1A; }
#endif

    virtual bool shouldShowPlaceholderWhenFocused() const override;
    virtual bool shouldHaveSpinButton(HTMLInputElement*) const override;

#if ENABLE(VIDEO)
    virtual String mediaControlsStyleSheet() override;
    virtual String mediaControlsScript() override;
#endif

private:
    RenderThemeIOS();
    virtual ~RenderThemeIOS() { }

    const Color& shadowColor() const;
    FloatRect addRoundedBorderClip(RenderObject* box, GraphicsContext*, const IntRect&);

    String m_mediaControlsScript;
    String m_mediaControlsStyleSheet;
    bool m_mediaControlsScriptLoaded;
    bool m_mediaControlsStyleSheetLoaded;
};

}

#endif // PLATFORM(IOS)
#endif // RenderThemeIOS_h
