/*
 * Copyright (C) 2019 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "ThemeTypes.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ControlPart controlPart)
{
    switch (controlPart) {
    case NoControlPart: ts << "no-control-part"; break;
    case CheckboxPart: ts << "checkbox-part"; break;
    case RadioPart: ts << "radio-part"; break;
    case PushButtonPart: ts << "push-button-part"; break;
    case SquareButtonPart: ts << "square-button-part"; break;
    case ButtonPart: ts << "button-part"; break;
    case ButtonBevelPart: ts << "button-bevel-part"; break;
    case DefaultButtonPart: ts << "default-button-part"; break;
    case InnerSpinButtonPart: ts << "inner-spin-button-part"; break;
    case ListboxPart: ts << "listbox-part"; break;
    case ListItemPart: ts << "list-item-part"; break;
    case MediaControlsBackgroundPart: ts << "media-controls-background-part"; break;
    case MediaControlsDarkBarBackgroundPart: ts << "media-controls-dark-bar-background-part"; break;
    case MediaControlsFullscreenBackgroundPart: ts << "media-controls-fullscreen-background-part"; break;
    case MediaControlsLightBarBackgroundPart: ts << "media-controls-light-bar-background-part"; break;
    case MediaCurrentTimePart: ts << "media-current-time-part"; break;
    case MediaEnterFullscreenButtonPart: ts << "media-enter-fullscreen-button-part"; break;
    case MediaExitFullscreenButtonPart: ts << "media-exit-fullscreen-button-part"; break;
    case MediaFullScreenVolumeSliderPart: ts << "media-full-screen-volume-slider-part"; break;
    case MediaFullScreenVolumeSliderThumbPart: ts << "media-full-screen-volume-slider-thumb-part"; break;
    case MediaMuteButtonPart: ts << "media-mute-button-part"; break;
    case MediaOverlayPlayButtonPart: ts << "media-overlay-play-button-part"; break;
    case MediaPlayButtonPart: ts << "media-play-button-part"; break;
    case MediaReturnToRealtimeButtonPart: ts << "media-return-to-realtime-button-part"; break;
    case MediaRewindButtonPart: ts << "media-rewind-button-part"; break;
    case MediaSeekBackButtonPart: ts << "media-seek-back-button-part"; break;
    case MediaSeekForwardButtonPart: ts << "media-seek-forward-button-part"; break;
    case MediaSliderPart: ts << "media-slider-part"; break;
    case MediaSliderThumbPart: ts << "media-slider-thumb-part"; break;
    case MediaTimeRemainingPart: ts << "media-time-remaining-part"; break;
    case MediaToggleClosedCaptionsButtonPart: ts << "media-toggle-closed-captions-button-part"; break;
    case MediaVolumeSliderPart: ts << "media-volume-slider-part"; break;
    case MediaVolumeSliderContainerPart: ts << "media-volume-slider-container-part"; break;
    case MediaVolumeSliderMuteButtonPart: ts << "media-volume-slider-mute-button-part"; break;
    case MediaVolumeSliderThumbPart: ts << "media-volume-slider-thumb-part"; break;
    case MenulistPart: ts << "menulist-part"; break;
    case MenulistButtonPart: ts << "menulist-button-part"; break;
    case MenulistTextPart: ts << "menulist-text-part"; break;
    case MenulistTextFieldPart: ts << "menulist-text-field-part"; break;
    case MeterPart: ts << "meter-part"; break;
    case ProgressBarPart: ts << "progress-bar-part"; break;
    case ProgressBarValuePart: ts << "progress-bar-value-part"; break;
    case SliderHorizontalPart: ts << "slider-horizontal-part"; break;
    case SliderVerticalPart: ts << "slider-vertical-part"; break;
    case SliderThumbHorizontalPart: ts << "slider-thumb-horizontal-part"; break;
    case SliderThumbVerticalPart: ts << "slider-thumb-vertical-part"; break;
    case CaretPart: ts << "caret-part"; break;
    case SearchFieldPart: ts << "search-field-part"; break;
    case SearchFieldDecorationPart: ts << "search-field-decoration-part"; break;
    case SearchFieldResultsDecorationPart: ts << "search-field-results-decoration-part"; break;
    case SearchFieldResultsButtonPart: ts << "search-field-results-button-part"; break;
    case SearchFieldCancelButtonPart: ts << "search-field-cancel-button-part"; break;
    case SnapshottedPluginOverlayPart: ts << "snapshotted-plugin-overlay-part"; break;
    case TextFieldPart: ts << "text-field-part"; break;
    case RelevancyLevelIndicatorPart: ts << "relevancy-level-indicator-part"; break;
    case ContinuousCapacityLevelIndicatorPart: ts << "continuous-capacity-level-indicator-part"; break;
    case DiscreteCapacityLevelIndicatorPart: ts << "discrete-capacity-level-indicator-part"; break;
    case RatingLevelIndicatorPart: ts << "rating-level-indicator-part"; break;
#if ENABLE(SERVICE_CONTROLS)
    case ImageControlsButtonPart: ts << "image-controls-button-part"; break;
#endif
#if ENABLE(APPLE_PAY)
    case ApplePayButtonPart: ts << "apple-pay-button-part"; break;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart: ts << "color-well-part"; break;
#endif
#if ENABLE(DATALIST_ELEMENT)
    case ListButtonPart: ts << "list-button-part"; break;
#endif
    case TextAreaPart: ts << "text-area-part"; break;
#if ENABLE(ATTACHMENT_ELEMENT)
    case AttachmentPart: ts << "attachment-part"; break;
    case BorderlessAttachmentPart: ts << "borderless-attachment-part"; break;
#endif
    case CapsLockIndicatorPart: ts << "caps-lock-indicator-part"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, SelectionPart selectionPart)
{
    switch (selectionPart) {
    case SelectionBackground: ts << "selection-background"; break;
    case SelectionForeground: ts << "selection-foreground"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ThemeFont themeFont)
{
    switch (themeFont) {
    case CaptionFont: ts << "caption-font"; break;
    case IconFont: ts << "icon-font"; break;
    case MenuFont: ts << "menu-font"; break;
    case MessageBoxFont: ts << "messagebox-font"; break;
    case SmallCaptionFont: ts << "small-caption-font"; break;
    case StatusBarFont: ts << "statusbar-font"; break;
    case MiniControlFont: ts << "minicontrol-font"; break;
    case SmallControlFont: ts << "small-control-font"; break;
    case ControlFont: ts << "control-font"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ThemeColor themeColor)
{
    switch (themeColor) {
    case ActiveBorderColor: ts << "active-border-color"; break;
    case ActiveCaptionColor: ts << "active-caption-color"; break;
    case ActiveTextColor: ts << "active-text-color"; break;
    case AppWorkspaceColor: ts << "app-workspace-color"; break;
    case BackgroundColor: ts << "background-color"; break;
    case ButtonFaceColor: ts << "button-face-color"; break;
    case ButtonHighlightColor: ts << "button-highlight-color"; break;
    case ButtonShadowColor: ts << "button-shadow-color"; break;
    case ButtonTextColor: ts << "button-text-color"; break;
    case CanvasColor: ts << "canvas-color"; break;
    case CanvasTextColor: ts << "canvas-text-color"; break;
    case CaptionTextColor: ts << "caption-text-color"; break;
    case FieldColor: ts << "field-color"; break;
    case FieldTextColor: ts << "field-text-color"; break;
    case GrayTextColor: ts << "gray-text-color"; break;
    case HighlightColor: ts << "highlight-color"; break;
    case HighlightTextColor: ts << "highlight-text-color"; break;
    case InactiveBorderColor: ts << "inactive-border-color"; break;
    case InactiveCaptionColor: ts << "inactive-caption-color"; break;
    case InactiveCaptionTextColor: ts << "inactive-caption-text-color"; break;
    case InfoBackgroundColor: ts << "info-background-color"; break;
    case InfoTextColor: ts << "info-text-color"; break;
    case LinkTextColor: ts << "link-text-color"; break;
    case MatchColor: ts << "match-color"; break;
    case MenuTextColor: ts << "menu-text-color"; break;
    case ScrollbarColor: ts << "scrollbar-color"; break;
    case ThreeDDarkShadowColor: ts << "threeD-dark-shadow-color"; break;
    case ThreeDFaceColor: ts << "threeD-face-color"; break;
    case ThreeDHighlightColor: ts << "threeD-highlight-color"; break;
    case ThreeDLightShadowColor: ts << "threeD-light-shadow-color"; break;
    case ThreeDShadowColor: ts << "threeD-shadow-color"; break;
    case VisitedTextColor: ts << "visited-text-color"; break;
    case WindowColor: ts << "window-color"; break;
    case WindowFrameColor: ts << "window-frame-color"; break;
    case WindowTextColor: ts << "window-text-color"; break;
    case FocusRingColor: ts << "focus-ring-color"; break;
    }
    return ts;
}

} // namespace WebCore
