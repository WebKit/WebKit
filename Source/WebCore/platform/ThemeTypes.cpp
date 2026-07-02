/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
    case SelectionPart::Background: ts << "selection-background"_s; break;
    case SelectionPart::Foreground: ts << "selection-foreground"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ThemeFont themeFont)
{
    switch (themeFont) {
    case ThemeFont::CaptionFont: ts << "caption-font"_s; break;
    case ThemeFont::IconFont: ts << "icon-font"_s; break;
    case ThemeFont::MenuFont: ts << "menu-font"_s; break;
    case ThemeFont::MessageBoxFont: ts << "messagebox-font"_s; break;
    case ThemeFont::SmallCaptionFont: ts << "small-caption-font"_s; break;
    case ThemeFont::StatusBarFont: ts << "statusbar-font"_s; break;
    case ThemeFont::MiniControlFont: ts << "minicontrol-font"_s; break;
    case ThemeFont::SmallControlFont: ts << "small-control-font"_s; break;
    case ThemeFont::ControlFont: ts << "control-font"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ThemeColor themeColor)
{
    switch (themeColor) {
    case ThemeColor::ActiveBorderColor: ts << "active-border-color"_s; break;
    case ThemeColor::ActiveCaptionColor: ts << "active-caption-color"_s; break;
    case ThemeColor::ActiveTextColor: ts << "active-text-color"_s; break;
    case ThemeColor::AppWorkspaceColor: ts << "app-workspace-color"_s; break;
    case ThemeColor::BackgroundColor: ts << "background-color"_s; break;
    case ThemeColor::ButtonFaceColor: ts << "button-face-color"_s; break;
    case ThemeColor::ButtonHighlightColor: ts << "button-highlight-color"_s; break;
    case ThemeColor::ButtonShadowColor: ts << "button-shadow-color"_s; break;
    case ThemeColor::ButtonTextColor: ts << "button-text-color"_s; break;
    case ThemeColor::CanvasColor: ts << "canvas-color"_s; break;
    case ThemeColor::CanvasTextColor: ts << "canvas-text-color"_s; break;
    case ThemeColor::CaptionTextColor: ts << "caption-text-color"_s; break;
    case ThemeColor::FieldColor: ts << "field-color"_s; break;
    case ThemeColor::FieldTextColor: ts << "field-text-color"_s; break;
    case ThemeColor::GrayTextColor: ts << "gray-text-color"_s; break;
    case ThemeColor::HighlightColor: ts << "highlight-color"_s; break;
    case ThemeColor::HighlightTextColor: ts << "highlight-text-color"_s; break;
    case ThemeColor::InactiveBorderColor: ts << "inactive-border-color"_s; break;
    case ThemeColor::InactiveCaptionColor: ts << "inactive-caption-color"_s; break;
    case ThemeColor::InactiveCaptionTextColor: ts << "inactive-caption-text-color"_s; break;
    case ThemeColor::InfoBackgroundColor: ts << "info-background-color"_s; break;
    case ThemeColor::InfoTextColor: ts << "info-text-color"_s; break;
    case ThemeColor::LinkTextColor: ts << "link-text-color"_s; break;
    case ThemeColor::MatchColor: ts << "match-color"_s; break;
    case ThemeColor::MenuTextColor: ts << "menu-text-color"_s; break;
    case ThemeColor::ScrollbarColor: ts << "scrollbar-color"_s; break;
    case ThemeColor::ThreeDDarkShadowColor: ts << "threeD-dark-shadow-color"_s; break;
    case ThemeColor::ThreeDFaceColor: ts << "threeD-face-color"_s; break;
    case ThemeColor::ThreeDHighlightColor: ts << "threeD-highlight-color"_s; break;
    case ThemeColor::ThreeDLightShadowColor: ts << "threeD-light-shadow-color"_s; break;
    case ThemeColor::ThreeDShadowColor: ts << "threeD-shadow-color"_s; break;
    case ThemeColor::VisitedTextColor: ts << "visited-text-color"_s; break;
    case ThemeColor::WindowColor: ts << "window-color"_s; break;
    case ThemeColor::WindowFrameColor: ts << "window-frame-color"_s; break;
    case ThemeColor::WindowTextColor: ts << "window-text-color"_s; break;
    case ThemeColor::FocusRingColor: ts << "focus-ring-color"_s; break;
    }
    return ts;
}

} // namespace WebCore
