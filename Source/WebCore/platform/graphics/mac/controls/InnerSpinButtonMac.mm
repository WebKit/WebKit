/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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
#import "InnerSpinButtonMac.h"

#if PLATFORM(MAC)

#import "GraphicsContext.h"
#import "InnerSpinButtonPart.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

InnerSpinButtonMac::InnerSpinButtonMac(InnerSpinButtonPart& owningPart, ControlFactoryMac& controlFactory)
    : ControlMac(owningPart, controlFactory)
{
}

IntSize InnerSpinButtonMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    static const IntSize sizes[] = {
        { 19, 27 },
        { 15, 22 },
        { 13, 15 },
        { 19, 27 }
    };
    return sizes[controlSize];
}

void InnerSpinButtonMac::draw(GraphicsContext& context, const FloatRect& rect, float, const ControlStyle& style)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    // We don't use NSStepperCell because there are no ways to draw an
    // NSStepperCell with the up button highlighted.

    NSString *coreUIState;
    auto states = style.states;
    if (!states.contains(ControlStyle::State::Enabled))
        coreUIState = (__bridge NSString *)kCUIStateDisabled;
    else if (states.contains(ControlStyle::State::Pressed))
        coreUIState = (__bridge NSString *)kCUIStatePressed;
    else
        coreUIState = (__bridge NSString *)kCUIStateActive;

    NSString *coreUISize;
    auto controlSize = controlSizeForSize(rect.size(), style);
    if (controlSize == NSControlSizeMini)
        coreUISize = (__bridge NSString *)kCUISizeMini;
    else if (controlSize == NSControlSizeSmall)
        coreUISize = (__bridge NSString *)kCUISizeSmall;
    else
        coreUISize = (__bridge NSString *)kCUISizeRegular;

    IntRect logicalRect(rect);

    GraphicsContextStateSaver stateSaver(context);
    if (style.zoomFactor != 1) {
        logicalRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    LocalCurrentGraphicsContext localContext(context);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[NSAppearance currentAppearance] _drawInRect:logicalRect context:localContext.cgContext() options:@{
    ALLOW_DEPRECATED_DECLARATIONS_END
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetButtonLittleArrows,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIStateKey: coreUIState,
        (__bridge NSString *)kCUIValueKey: states.contains(ControlStyle::State::SpinUp) ? @1 : @0,
        (__bridge NSString *)kCUIIsFlippedKey: @NO,
        (__bridge NSString *)kCUIScaleKey: @1,
        (__bridge NSString *)kCUIMaskOnlyKey: @NO
    }];

    END_BLOCK_OBJC_EXCEPTIONS
}

} // namespace WebCore

#endif // PLATFORM(MAC)
