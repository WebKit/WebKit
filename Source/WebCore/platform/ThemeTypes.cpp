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
