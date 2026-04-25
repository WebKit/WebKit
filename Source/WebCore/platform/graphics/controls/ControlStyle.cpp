/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
        ts << "hovered"_s;
        break;
    case ControlStyle::State::Pressed:
        ts << "pressed"_s;
        break;
    case ControlStyle::State::Focused:
        ts << "focused"_s;
        break;
    case ControlStyle::State::Enabled:
        ts << "enabled"_s;
        break;
    case ControlStyle::State::Checked:
        ts << "checked"_s;
        break;
    case ControlStyle::State::Default:
        ts << "default"_s;
        break;
    case ControlStyle::State::WindowActive:
        ts << "window-active"_s;
        break;
    case ControlStyle::State::Indeterminate:
        ts << "indeterminate"_s;
        break;
    case ControlStyle::State::SpinUp:
        ts << "spin-up"_s;
        break;
    case ControlStyle::State::Presenting:
        ts << "presenting"_s;
        break;
    case ControlStyle::State::FormSemanticContext:
        ts << "form-semantic-context"_s;
        break;
    case ControlStyle::State::DarkAppearance:
        ts << "dark-appearance"_s;
        break;
    case ControlStyle::State::InlineFlippedWritingMode:
        ts << "inline-flipped-writing-mode"_s;
        break;
    case ControlStyle::State::LargeControls:
        ts << "large-controls"_s;
        break;
    case ControlStyle::State::ReadOnly:
        ts << "read-only"_s;
        break;
    case ControlStyle::State::ListButton:
        ts << "list-button"_s;
        break;
    case ControlStyle::State::ListButtonPressed:
        ts << "list-button-pressed"_s;
        break;
    case ControlStyle::State::VerticalWritingMode:
        ts << "vertical-writing-mode"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const ControlStyle& style)
{
    ts.dumpProperty("states"_s, style.states);
    ts.dumpProperty("font-size"_s, style.fontSize);
    ts.dumpProperty("zoom-factor"_s, style.zoomFactor);
    ts.dumpProperty("accent-color"_s, style.accentColor);
    return ts;
}

} // namespace WebCore
