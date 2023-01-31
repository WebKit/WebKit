/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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

#import "config.h"
#import "TextFieldMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "LocalCurrentGraphicsContext.h"
#import "TextFieldPart.h"
#import "WebControlView.h"

namespace WebCore {

TextFieldMac::TextFieldMac(TextFieldPart& owningPart, ControlFactoryMac& controlFactory, NSTextFieldCell* textFieldCell)
    : ControlMac(owningPart, controlFactory)
    , m_textFieldCell(textFieldCell)
{
    ASSERT(m_textFieldCell);
}

bool TextFieldMac::shouldPaintCustomTextField(const ControlStyle& style)
{
    // <rdar://problem/88948646> Prevent AppKit from painting text fields in the light appearance
    // with increased contrast, as the border is not painted, rendering the control invisible.
    return userPrefersContrast() && !style.states.contains(ControlStyle::State::DarkAppearance);
}

void TextFieldMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    const auto& states = style.states;
    auto enabled = states.contains(ControlStyle::State::Enabled) && !states.contains(ControlStyle::State::ReadOnly);

    LocalCurrentGraphicsContext localContext(context);
    GraphicsContextStateSaver stateSaver(context);

    FloatRect paintRect(borderRect.rect());

    if (shouldPaintCustomTextField(style)) {
        constexpr int strokeThickness = 1;

        FloatRect strokeRect(paintRect);
        strokeRect.inflate(-strokeThickness / 2.0f);

        context.setStrokeColor(enabled ? Color::black : Color::darkGray);
        context.setStrokeStyle(StrokeStyle::SolidStroke);
        context.strokeRect(strokeRect, strokeThickness);
    } else {
        // <rdar://problem/22896977> We adjust the paint rect here to account for how AppKit draws the text
        // field cell slightly smaller than the rect we pass to drawWithFrame.
        AffineTransform transform = context.getCTM();
        if (transform.xScale() > 1 || transform.yScale() > 1) {
            paintRect.inflateX(1 / transform.xScale());
            paintRect.inflateY(2 / transform.yScale());
            paintRect.move(0, -1 / transform.yScale());
        }
        
        auto *view = m_controlFactory.drawingView(borderRect.rect(), style);
        
        [m_textFieldCell.get() setEnabled:enabled];
        [m_textFieldCell.get() drawWithFrame:NSRect(paintRect) inView:view];
        [m_textFieldCell.get() setControlView:nil];
    }

#if ENABLE(DATALIST_ELEMENT)
    drawListButton(context, borderRect.rect(), deviceScaleFactor, style);
#endif
}

} // namespace WebCore

#endif // PLATFORM(MAC)
