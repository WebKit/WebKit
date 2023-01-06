/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "ControlStyle.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ControlStyle::State state)
{
    switch (state) {
    case ControlStyle::State::Hovered:
        ts << "hovered";
        break;
    case ControlStyle::State::Pressed:
        ts << "pressed";
        break;
    case ControlStyle::State::Focused:
        ts << "focused";
        break;
    case ControlStyle::State::Enabled:
        ts << "enabled";
        break;
    case ControlStyle::State::Checked:
        ts << "checked";
        break;
    case ControlStyle::State::Default:
        ts << "default";
        break;
    case ControlStyle::State::WindowActive:
        ts << "window-active";
        break;
    case ControlStyle::State::Indeterminate:
        ts << "indeterminate";
        break;
    case ControlStyle::State::SpinUp:
        ts << "spin-up";
        break;
    case ControlStyle::State::Presenting:
        ts << "presenting";
        break;
    case ControlStyle::State::FormSemanticContext:
        ts << "form-semantic-context";
        break;
    case ControlStyle::State::DarkAppearance:
        ts << "dark-appearance";
        break;
    case ControlStyle::State::RightToLeft:
        ts << "right-to-left";
        break;
    case ControlStyle::State::LargeControls:
        ts << "large-controls";
        break;
    case ControlStyle::State::ReadOnly:
        ts << "read-only";
        break;
    case ControlStyle::State::ListButton:
        ts << "list-button";
        break;
    case ControlStyle::State::ListButtonPressed:
        ts << "list-button-pressed";
        break;
    case ControlStyle::State::VerticalWritingMode:
        ts << "vertical-writing-mode";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const ControlStyle& style)
{
    ts.dumpProperty("states", style.states);
    ts.dumpProperty("font-size", style.fontSize);
    ts.dumpProperty("zoom-factor", style.zoomFactor);
    ts.dumpProperty("accent-color", style.accentColor);
    return ts;
}

} // namespace WebCore
