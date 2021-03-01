/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
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

#pragma once

namespace WTF {
class TextStream;
}

namespace WebCore {

// Must follow CSSValueKeywords.in order
enum ControlPart {
    NoControlPart,
    CheckboxPart,
    RadioPart,
    PushButtonPart,
    SquareButtonPart,
    ButtonPart,
    ButtonBevelPart,
    DefaultButtonPart,
    InnerSpinButtonPart,
    ListboxPart,
    ListItemPart,
    MediaControlsBackgroundPart,
    MediaControlsDarkBarBackgroundPart,
    MediaControlsFullscreenBackgroundPart,
    MediaControlsLightBarBackgroundPart,
    MediaCurrentTimePart,
    MediaEnterFullscreenButtonPart,
    MediaExitFullscreenButtonPart,
    MediaFullScreenVolumeSliderPart,
    MediaFullScreenVolumeSliderThumbPart,
    MediaMuteButtonPart,
    MediaOverlayPlayButtonPart,
    MediaPlayButtonPart,
    MediaReturnToRealtimeButtonPart,
    MediaRewindButtonPart,
    MediaSeekBackButtonPart,
    MediaSeekForwardButtonPart,
    MediaSliderPart,
    MediaSliderThumbPart,
    MediaTimeRemainingPart,
    MediaToggleClosedCaptionsButtonPart,
    MediaVolumeSliderPart,
    MediaVolumeSliderContainerPart,
    MediaVolumeSliderMuteButtonPart,
    MediaVolumeSliderThumbPart,
    MenulistPart,
    MenulistButtonPart,
    MenulistTextPart,
    MenulistTextFieldPart,
    MeterPart,
    ProgressBarPart,
    ProgressBarValuePart,
    SliderHorizontalPart,
    SliderVerticalPart,
    SliderThumbHorizontalPart,
    SliderThumbVerticalPart,
    CaretPart,
    SearchFieldPart,
    SearchFieldDecorationPart,
    SearchFieldResultsDecorationPart,
    SearchFieldResultsButtonPart,
    SearchFieldCancelButtonPart,
    TextFieldPart,
    RelevancyLevelIndicatorPart,
    ContinuousCapacityLevelIndicatorPart,
    DiscreteCapacityLevelIndicatorPart,
    RatingLevelIndicatorPart,
#if ENABLE(APPLE_PAY)
    ApplePayButtonPart,
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    ColorWellPart,
#endif
#if ENABLE(DATALIST_ELEMENT)
    ListButtonPart,
#endif
    TextAreaPart,
#if ENABLE(ATTACHMENT_ELEMENT)
    AttachmentPart,
    BorderlessAttachmentPart,
#endif
    CapsLockIndicatorPart
};

enum SelectionPart {
    SelectionBackground,
    SelectionForeground
};

enum ThemeFont {
    CaptionFont,
    IconFont,
    MenuFont,
    MessageBoxFont,
    SmallCaptionFont,
    StatusBarFont,
    MiniControlFont,
    SmallControlFont,
    ControlFont
};

enum ThemeColor {
    ActiveBorderColor,
    ActiveCaptionColor,
    ActiveTextColor,
    AppWorkspaceColor,
    BackgroundColor,
    ButtonFaceColor,
    ButtonHighlightColor,
    ButtonShadowColor,
    ButtonTextColor,
    CanvasColor,
    CanvasTextColor,
    CaptionTextColor,
    FieldColor,
    FieldTextColor,
    GrayTextColor,
    HighlightColor,
    HighlightTextColor,
    InactiveBorderColor,
    InactiveCaptionColor,
    InactiveCaptionTextColor,
    InfoBackgroundColor,
    InfoTextColor,
    LinkTextColor,
    MatchColor,
    MenuTextColor,
    ScrollbarColor,
    ThreeDDarkShadowColor,
    ThreeDFaceColor,
    ThreeDHighlightColor,
    ThreeDLightShadowColor,
    ThreeDShadowColor,
    VisitedTextColor,
    WindowColor,
    WindowFrameColor,
    WindowTextColor,
    FocusRingColor
};

WTF::TextStream& operator<<(WTF::TextStream&, ControlPart);
WTF::TextStream& operator<<(WTF::TextStream&, SelectionPart);
WTF::TextStream& operator<<(WTF::TextStream&, ThemeFont);
WTF::TextStream& operator<<(WTF::TextStream&, ThemeColor);

} // namespace WebCore
