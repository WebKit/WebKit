/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#include "CSSValueKey.h"
#include "RenderThemeCocoa.h"

OBJC_CLASS UIImage;

namespace WebCore {
    
class RenderStyle;
class GraphicsContext;

struct AttachmentLayout;

class RenderThemeIOS final : public RenderThemeCocoa {
public:
    friend NeverDestroyed<RenderThemeIOS>;

    static void adjustRoundBorderRadius(RenderStyle&, RenderBox&);

#if USE(SYSTEM_PREVIEW)
    void paintSystemPreviewBadge(Image&, const PaintInfo&, const FloatRect&) override;
#endif

    using CSSValueToSystemColorMap = HashMap<CSSValueKey, Color>;

    WEBCORE_EXPORT static const CSSValueToSystemColorMap& cssValueToSystemColorMap();
    WEBCORE_EXPORT static void setCSSValueToSystemColorMap(CSSValueToSystemColorMap&&);

    WEBCORE_EXPORT static void setFocusRingColor(const Color&);

    WEBCORE_EXPORT static Color systemFocusRingColor();

    struct IconAndSize {
        RetainPtr<UIImage> icon;
        FloatSize size;
    };

    WEBCORE_EXPORT static IconAndSize iconForAttachment(const String& fileName, const String& attachmentType, const String& title);

private:
    bool canPaint(const PaintInfo&, const Settings&) const final;

    LengthBox popupInternalPaddingBox(const RenderStyle&, const Settings&) const override;

    LayoutRect adjustedPaintRect(const RenderBox&, const LayoutRect&) const override;

    int baselinePosition(const RenderBox&) const override;

    bool isControlStyled(const RenderStyle&, const RenderStyle& userAgentStyle) const override;

    // Methods for each appearance value.
    void adjustCheckboxStyle(RenderStyle&, const Element*) const override;
    void paintCheckboxDecorations(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustRadioStyle(RenderStyle&, const Element*) const override;
    void paintRadioDecorations(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustButtonStyle(RenderStyle&, const Element*) const override;
    void paintButtonDecorations(const RenderObject&, const PaintInfo&, const IntRect&) override;
    void paintPushButtonDecorations(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustTextFieldStyle(RenderStyle&, const Element*) const final;
    void paintTextFieldDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;
    void adjustTextAreaStyle(RenderStyle&, const Element*) const final;
    void paintTextAreaDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void paintTextFieldInnerShadow(const PaintInfo&, const FloatRoundedRect&);

    void adjustMenuListButtonStyle(RenderStyle&, const Element*) const override;
    void paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustSliderTrackStyle(RenderStyle&, const Element*) const override;
    bool paintSliderTrack(const RenderObject&, const PaintInfo&, const IntRect&) override;

    void adjustSliderThumbSize(RenderStyle&, const Element*) const override;
    void paintSliderThumbDecorations(const RenderObject&, const PaintInfo&, const IntRect&) override;

    bool paintProgressBar(const RenderObject&, const PaintInfo&, const IntRect&) override;

#if ENABLE(DATALIST_ELEMENT)
    IntSize sliderTickSize() const override;
    int sliderTickOffsetFromTrackCenter() const override;
#endif

    void adjustSearchFieldStyle(RenderStyle&, const Element*) const override;
    void paintSearchFieldDecorations(const RenderBox&, const PaintInfo&, const IntRect&) override;

#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    Color checkboxRadioBorderColor(OptionSet<ControlStates::States>, OptionSet<StyleColorOptions>);
    Color checkboxRadioBackgroundColor(bool useAlternateDesign, const RenderStyle&, OptionSet<ControlStates::States>, OptionSet<StyleColorOptions>);
    RefPtr<Gradient> checkboxRadioBackgroundGradient(const FloatRect&, OptionSet<ControlStates::States>);
    Color checkboxRadioIndicatorColor(OptionSet<ControlStates::States>, OptionSet<StyleColorOptions>);

    bool paintCheckbox(const RenderObject&, const PaintInfo&, const FloatRect&) override;
    bool paintRadio(const RenderObject&, const PaintInfo&, const FloatRect&) override;

    void paintCheckboxRadioInnerShadow(const PaintInfo&, const FloatRoundedRect&, OptionSet<ControlStates::States>);

    Seconds animationRepeatIntervalForProgressBar(const RenderProgress&) const final;

    bool supportsMeter(ControlPart, const HTMLMeterElement&) const final;
    bool paintMeter(const RenderObject&, const PaintInfo&, const IntRect&) final;

#if ENABLE(DATALIST_ELEMENT)
    bool paintListButton(const RenderObject&, const PaintInfo&, const FloatRect&) final;

    void paintSliderTicks(const RenderObject&, const PaintInfo&, const FloatRect&) final;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    String colorInputStyleSheet(const Settings&) const final;

    void adjustColorWellStyle(RenderStyle&, const Element*) const final;
    bool paintColorWell(const RenderObject&, const PaintInfo&, const IntRect&) final;
    void paintColorWellDecorations(const RenderObject&, const PaintInfo&, const FloatRect&) final;
#endif

    void adjustSearchFieldDecorationPartStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldDecorationPart(const RenderObject&, const PaintInfo&, const IntRect&) final;

    void adjustSearchFieldResultsDecorationPartStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const IntRect&) final;

    void adjustSearchFieldResultsButtonStyle(RenderStyle&, const Element*) const final;
    bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const IntRect&) final;
#endif

    bool supportsFocusRing(const RenderStyle&) const final;

    bool supportsBoxShadow(const RenderStyle&) const final;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const override;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const override;
    Color platformFocusRingColor(OptionSet<StyleColorOptions>) const final;

    Color platformAnnotationHighlightColor(OptionSet<StyleColorOptions>) const final;

#if ENABLE(TOUCH_EVENTS)
    Color platformTapHighlightColor() const override { return SRGBA<uint8_t> { 26, 26, 26, 77 } ; }
#endif

    bool shouldHaveSpinButton(const HTMLInputElement&) const override;

#if ENABLE(ATTACHMENT_ELEMENT)
    LayoutSize attachmentIntrinsicSize(const RenderAttachment&) const override;
    bool attachmentShouldAllowWidthToShrink(const RenderAttachment&) const override { return true; }
    String attachmentStyleSheet() const final;
    bool paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&) override;
#endif

private:
    RenderThemeIOS();
    virtual ~RenderThemeIOS() = default;

#if PLATFORM(WATCHOS)
    String extraDefaultStyleSheet() final;
#endif

#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    bool paintProgressBarWithFormControlRefresh(const RenderObject&, const PaintInfo&, const IntRect&);
    bool paintSliderTrackWithFormControlRefresh(const RenderObject&, const PaintInfo&, const IntRect&);
    void paintMenuListButtonDecorationsWithFormControlRefresh(const RenderBox&, const PaintInfo&, const FloatRect&);
#endif

    bool isSubmitStyleButton(const Element&) const;

    void adjustButtonLikeControlStyle(RenderStyle&, const Element&) const;

    FloatRect addRoundedBorderClip(const RenderObject& box, GraphicsContext&, const IntRect&);

    Color systemColor(CSSValueID, OptionSet<StyleColorOptions>) const override;

    Color pictureFrameColor(const RenderObject&) override;

    Color controlTintColor(const RenderStyle&, OptionSet<StyleColorOptions>) const;

    void adjustStyleForAlternateFormControlDesignTransition(RenderStyle&, const Element*) const;
};

}

#endif // PLATFORM(IOS_FAMILY)
