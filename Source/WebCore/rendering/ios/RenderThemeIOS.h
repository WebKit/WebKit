/*
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#if PLATFORM(IOS_FAMILY)

#include <WebCore/CSSValueKey.h>
#include <WebCore/RenderThemeCocoa.h>

OBJC_CLASS UIImage;

namespace WebCore {

class GraphicsContext;
struct AttachmentLayout;

namespace Style {
class ComputedStyle;
}

class RenderThemeIOS final : public RenderThemeCocoa {
public:
    friend NeverDestroyed<RenderThemeIOS>;

    static void adjustRoundBorderRadius(Style::ComputedStyle&, RenderBox&);

#if USE(SYSTEM_PREVIEW)
    void paintSystemPreviewBadge(Image&, const PaintInfo&, const FloatRect&) final;
    void paintSystemPreviewBadge(const PaintInfo&, const FloatRect&) final;
#endif

    using CSSValueToSystemColorMap = HashMap<CSSValueKey, Color>;

    WEBCORE_EXPORT static const CSSValueToSystemColorMap& cssValueToSystemColorMap();
    WEBCORE_EXPORT static void setCSSValueToSystemColorMap(CSSValueToSystemColorMap&&);

    WEBCORE_EXPORT static void setFocusRingColor(const Color&);
    WEBCORE_EXPORT static void setInsertionPointColor(const Color&);

    WEBCORE_EXPORT static Color systemFocusRingColor();

    WEBCORE_EXPORT static IconAndSize iconForAttachment(const String& fileName, const String& attachmentType, const String& title);

    bool canCreateControlPartForRenderer(const RenderElement&) const final;

    Style::PaddingBox platformPopupInternalPaddingBox(const Style::ComputedStyle&) const final;

    int baselinePosition(const RenderBox&) const final;

    bool isControlStyled(const Style::ComputedStyle&) const final;

    // Methods for each appearance value.
    void adjustCheckboxStyle(Style::ComputedStyle&, const Element*) const final;

    void adjustRadioStyle(Style::ComputedStyle&, const Element*) const final;

    void adjustButtonStyle(Style::ComputedStyle&, const Element*) const final;

    void adjustInnerSpinButtonStyle(Style::ComputedStyle&, const Element*) const final { }

    void adjustTextFieldStyle(Style::ComputedStyle&, const Element*) const final;
    void paintTextFieldDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;
    void adjustTextAreaStyle(Style::ComputedStyle&, const Element*) const final;
    void paintTextAreaDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    void adjustMenuListButtonStyle(Style::ComputedStyle&, const Element*) const final;
    void paintMenuListButtonDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    void adjustSliderTrackStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintSliderTrack(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustSliderThumbSize(Style::ComputedStyle&, const Element*) const final;

    Seconds switchAnimationVisuallyOnDuration() const final { return 0.4880138408543766_s; }
    Seconds switchAnimationHeldDuration() const final { return 0.5073965509413827_s; }
#if HAVE(UI_IMPACT_FEEDBACK_GENERATOR)
    bool hasSwitchHapticFeedback(SwitchTrigger) const final { return true; }
#endif

    bool paintProgressBar(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    IntSize sliderTickSize() const final;
    int sliderTickOffsetFromTrackCenter() const final;

    void adjustSearchFieldStyle(Style::ComputedStyle&, const Element*) const final;
    void paintSearchFieldDecorations(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    bool paintCheckbox(const RenderElement&, const PaintInfo&, const FloatRect&) final;
    bool paintRadio(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    bool supportsMeter(StyleAppearance) const final;
    bool paintMeter(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    bool paintListButton(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void paintSliderTicks(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void paintColorWellDecorations(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustSearchFieldDecorationPartStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintSearchFieldDecorationPart(const RenderElement&, const PaintInfo&, const FloatRect&) final;

    void adjustSearchFieldResultsDecorationPartStyle(Style::ComputedStyle&, const Element*) const final;

    bool paintSearchFieldResultsDecorationPart(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    void adjustSearchFieldResultsButtonStyle(Style::ComputedStyle&, const Element*) const final;
    bool paintSearchFieldResultsButton(const RenderBox&, const PaintInfo&, const FloatRect&) final;

    bool supportsFocusRing(const RenderElement&, const Style::ComputedStyle&) const final;

    bool supportsBoxShadow(const Style::ComputedStyle&) const final;

    Color autocorrectionReplacementMarkerColor(const RenderText&) const final;

    Color platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const final;
    Color platformFocusRingColor(OptionSet<StyleColorOptions>) const final;

    Color platformAnnotationHighlightBackgroundColor(OptionSet<StyleColorOptions>) const final;

    Color platformTextSearchHighlightColor(OptionSet<StyleColorOptions>) const final;

#if ENABLE(CSS_TAP_HIGHLIGHT_COLOR)
    Color platformTapHighlightColor() const final { return SRGBA<uint8_t> { 26, 26, 26, 77 } ; }
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    LayoutSize attachmentIntrinsicSize(const RenderAttachment&) const final;
    bool attachmentShouldAllowWidthToShrink(const RenderAttachment&) const final { return true; }
    String attachmentStyleSheet() const final;
    bool paintAttachment(const RenderElement&, const PaintInfo&, const IntRect&) final;
#endif

    bool shouldHaveSpinButton(const HTMLInputElement&) const final;

#if PLATFORM(WATCHOS)
    String extraDefaultStyleSheet() final;
#endif

    WEBCORE_EXPORT Color systemColor(CSSValueID, OptionSet<StyleColorOptions>) const final;
    Color pictureFrameColor(const RenderElement&) final;

    Style::PreferredSizePair controlSize(StyleAppearance, const FontCascade&, const Style::PreferredSizePair&, float zoomFactor) const final;

private:
    RenderThemeIOS();
    virtual ~RenderThemeIOS();

    void paintTextFieldInnerShadow(const PaintInfo&, const FloatRoundedRect&);

    Color checkboxRadioBorderColor(OptionSet<ControlStyle::State>, OptionSet<StyleColorOptions>);
    Color checkboxRadioBackgroundColor(const Style::ComputedStyle&, OptionSet<ControlStyle::State>, OptionSet<StyleColorOptions>);
    RefPtr<Gradient> checkboxRadioBackgroundGradient(const FloatRect&, OptionSet<ControlStyle::State>);
    Color checkboxRadioIndicatorColor(OptionSet<ControlStyle::State>, OptionSet<StyleColorOptions>);

    void paintCheckboxRadioInnerShadow(const PaintInfo&, const FloatRoundedRect&, OptionSet<ControlStyle::State>);

    static Color insertionPointColor();

    void adjustButtonLikeControlStyle(Style::ComputedStyle&, const Element&) const;

    void adjustMinimumIntrinsicSizeForAppearance(StyleAppearance, Style::ComputedStyle&) const;
};

}

#endif // PLATFORM(IOS_FAMILY)
