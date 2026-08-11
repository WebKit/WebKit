/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include <WebCore/Icon.h>
#include <WebCore/RenderTheme.h>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSDateComponentsFormatter;
struct AttachmentLayout;

namespace WebCore {

class Node;

#if ENABLE(FORM_CONTROL_REFRESH)

enum class CornerType : uint8_t {
    Noncontinuous,
    Continuous
};

struct RoundedShape {
    std::optional<Path> path;
    FloatRect boundingRect;
    float cornerRadius = 0;
    CornerType cornerType = CornerType::Noncontinuous;
};

enum class ShouldComputePath : bool  {
    No,
    Yes
};

#endif

class RenderThemeCocoa : public RenderTheme {
public:
    Color controlTintColor(const Style::ComputedStyle&, OptionSet<StyleColorOptions>) const;

    void adjustRepaintRect(const RenderBox&, FloatRect&) final;

#if ENABLE(FORM_CONTROL_REFRESH)
    Color controlTintColorWithContrast(const Style::ComputedStyle&, OptionSet<StyleColorOptions>) const;
    static std::optional<RoundedShape> shapeForInteractionRegion(const RenderBox&, const FloatRect&, ShouldComputePath);
    static FloatSize inflateRectForInteractionRegion(const RenderElement&, FloatRect&);
    bool controlSupportsTints(const RenderElement&) const override;
    bool supportsControlTints() const override { return true; }
#endif

    struct IconAndSize {
#if PLATFORM(IOS_FAMILY)
        RetainPtr<UIImage> icon;
#else
        RetainPtr<NSImage> icon;
#endif
        FloatSize size;
    };

#if ENABLE(VIDEO)
    Vector<String, 2> mediaControlsStyleSheets(const HTMLMediaElement&) final;
    String mediaControlsBase64StringForIconNameAndType(const String&, const String&) final;
    Vector<String, 2> mediaControlsScripts() final;
    RefPtr<FragmentedSharedBuffer> mediaControlsImageDataForIconNameAndType(const String&, const String&) final;
    String mediaControlsFormattedStringForDuration(double) final;
    String youTubeQuirkScript() final;
    String cnnQuirkScript() final;
#endif

#if ENABLE(FORM_CONTROL_REFRESH)
    Color submitButtonTextColor(const RenderText&) const final;
    float adjustedMaximumLogicalWidthForControl(const Style::ComputedStyle&, const Element&, float) const final;
#endif

    void purgeCaches() final;
    void adjustTextControlInnerContainerStyle(Style::ComputedStyle&, const Style::ComputedStyle&, const Element*) const final;
    void adjustTextControlInnerTextStyle(Style::ComputedStyle&, const Style::ComputedStyle&, const Element*) const final;
    void adjustTextControlInnerPlaceholderStyle(Style::ComputedStyle&, const Style::ComputedStyle&, const Element*) const final;
    bool shouldHaveCapsLockIndicator(const HTMLInputElement&) const final;

protected:
    virtual Color pictureFrameColor(const RenderElement&);
#if ENABLE(ATTACHMENT_ELEMENT)
    int attachmentBaseline(const RenderAttachment&) const final;
    void paintAttachmentText(GraphicsContext&, AttachmentLayout*) final;
#endif

    void inflateRectForControlRenderer(const RenderElement&, FloatRect&) override;

    Style::LineWidthBox controlBorder(StyleAppearance, const FontCascade&, const Style::LineWidthBox& zoomedBox, float zoomFactor, const Element*) const override;

    Color platformSpellingMarkerColor(OptionSet<StyleColorOptions>) const final;
    Color platformDictationAlternativesMarkerColor(OptionSet<StyleColorOptions>) const final;
    Color platformGrammarMarkerColor(OptionSet<StyleColorOptions>) const final;

    void adjustCheckboxStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintCheckbox(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustRadioStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintRadio(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustButtonStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintButton(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustColorWellStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintColorWell(const RenderElement&, const PaintInfo&, const FloatRect&) final;
    void paintColorWellDecorations(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustColorWellSwatchStyle(Style::ComputedStyle&, const Element*) const final;
    void adjustColorWellSwatchOverlayStyle(Style::ComputedStyle&, const Element*) const final;
    void adjustColorWellSwatchWrapperStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintColorWellSwatch(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustInnerSpinButtonStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintInnerSpinButton(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustTextFieldStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintTextField(const RenderElement&, const PaintInfo&, const FloatRect&) final;
    void paintTextFieldDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustTextAreaStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintTextArea(const RenderElement&, const PaintInfo&, const FloatRect&) final;
    void paintTextAreaDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustMenuListStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintMenuList(const RenderElement&, const PaintInfo&, const FloatRect&) final;
    void paintMenuListDecorations(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustMenuListButtonStyle(Style::ComputedStyle&, const Element*) const override;
    void paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;
    bool paintMenuListButton(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustMeterStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintMeter(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustListButtonStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintListButton(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustProgressBarStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintProgressBar(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustSliderTrackStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSliderTrack(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustSliderThumbSize(Style::ComputedStyle&, const Element*) const override;
    void adjustSliderThumbStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSliderThumb(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustSearchFieldStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSearchField(const RenderElement&, const PaintInfo&, const FloatRect&) final;
    void paintSearchFieldDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustSearchFieldCancelButtonStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSearchFieldCancelButton(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    void adjustSearchFieldDecorationPartStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSearchFieldDecorationPart(const RenderElement&, const PaintInfo&, const FloatRect&) override;

    void adjustSearchFieldResultsDecorationPartStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustSearchFieldResultsButtonStyle(Style::ComputedStyle&, const Element*) const override;
    bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const FloatRect&) override;

    void adjustSwitchStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintSwitch(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void paintPlatformResizer(const RenderLayerModelObject&, GraphicsContext&, const LayoutRect&) final;
    void paintPlatformResizerFrame(const RenderLayerModelObject&, GraphicsContext&, const LayoutRect&) final;

    bool supportsFocusRing(const RenderElement&, const Style::ComputedStyle&) const override;

#if ENABLE(FORM_CONTROL_REFRESH)
    bool NODELETE inflateRectForControlRendererForVectorBasedControls(const RenderElement& renderer, FloatRect&) const;

    bool NODELETE canCreateControlPartForRendererForVectorBasedControls(const RenderElement&) const;
    bool NODELETE canCreateControlPartForBorderOnlyForVectorBasedControls(const RenderElement&) const;
    bool NODELETE canCreateControlPartForDecorationsForVectorBasedControls(const RenderElement&) const;

    Color checkboxRadioBackgroundColorForVectorBasedControls(const Style::ComputedStyle&, OptionSet<ControlStyle::State>, OptionSet<StyleColorOptions>) const;

    bool paintCheckboxForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool paintRadioForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustButtonStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintButtonForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustColorWellStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintColorWellForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);
    bool paintColorWellDecorationsForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool NODELETE adjustColorWellSwatchStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool NODELETE adjustColorWellSwatchOverlayStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool adjustColorWellSwatchWrapperStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintColorWellSwatchForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustInnerSpinButtonStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintInnerSpinButtonForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustTextFieldStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintTextFieldForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);
    bool NODELETE paintTextFieldDecorationsForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&);

    bool adjustTextAreaStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintTextAreaForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);
    bool NODELETE paintTextAreaDecorationsForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&);

    bool adjustMenuListStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintMenuListForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);
    bool NODELETE paintMenuListDecorationsForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustMenuListButtonStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintMenuListButtonForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);
    bool paintMenuListButtonDecorationsForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool NODELETE adjustMeterStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintMeterForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustListButtonStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintListButtonForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool NODELETE adjustProgressBarStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintProgressBarForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool NODELETE adjustSliderTrackStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSliderTrackForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustSliderThumbSizeForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool adjustSliderThumbStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSliderThumbForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustSearchFieldStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSearchFieldForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);
    bool NODELETE paintSearchFieldDecorationsForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&);

    bool adjustSearchFieldCancelButtonStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSearchFieldCancelButtonForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&);

    bool adjustSearchFieldDecorationPartStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSearchFieldDecorationPartForVectorBasedControls(const RenderElement&, const PaintInfo&, const FloatRect&);

    bool adjustSearchFieldResultsDecorationPartStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSearchFieldResultsDecorationPartForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&);

    bool adjustSearchFieldResultsButtonStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;
    bool paintSearchFieldResultsButtonForVectorBasedControls(const RenderBox&, const PaintInfo&, const FloatRect&);

    bool adjustSwitchStyleForVectorBasedControls(Style::ComputedStyle&, const Element*) const;

    bool paintPlatformResizerForVectorBasedControls(const RenderLayerModelObject&, GraphicsContext&, const LayoutRect&);
    bool NODELETE paintPlatformResizerFrameForVectorBasedControls(const RenderLayerModelObject&, GraphicsContext&, const LayoutRect&);

    bool NODELETE supportsFocusRingForVectorBasedControls(const RenderElement&, const Style::ComputedStyle&) const;

    bool NODELETE adjustTextControlInnerContainerStyleForVectorBasedControls(Style::ComputedStyle&, const Style::ComputedStyle&, const Element*) const;
    bool adjustTextControlInnerPlaceholderStyleForVectorBasedControls(Style::ComputedStyle&, const Style::ComputedStyle&, const Element*) const;
    bool adjustTextControlInnerTextStyleForVectorBasedControls(Style::ComputedStyle&, const Style::ComputedStyle&, const Element*) const;

    Color buttonTextColor(OptionSet<StyleColorOptions>, bool) const;

    bool mayNeedBleedAvoidance(const Style::ComputedStyle&) const final;
#endif

    bool isSubmitStyleButton(const Node*) const;

private:
    void paintFileUploadIconDecorations(const RenderElement& inputRenderer, const RenderElement& buttonRenderer, const PaintInfo&, const FloatRect&, Icon*, FileUploadDecorations) final;

    Seconds animationRepeatIntervalForProgressBar(const RenderProgress&) const final;

#if ENABLE(APPLE_PAY)
    void adjustApplePayButtonStyle(Style::ComputedStyle&, const Element*) const final;
#endif

    LayoutRect adjustedPaintRect(const RenderBox&, const LayoutRect&) const final;

#if ENABLE(VIDEO)
    String m_mediaControlsLocalizedStringsScript;
    String m_mediaControlsScript;
    String m_mediaControlsStyleSheet;
    RetainPtr<NSDateComponentsFormatter> m_durationFormatter;
    String m_youTubeCaptionQuirkScript;
    String m_cnnCaptionQuirkScript;
#endif // ENABLE(VIDEO)
};

}
