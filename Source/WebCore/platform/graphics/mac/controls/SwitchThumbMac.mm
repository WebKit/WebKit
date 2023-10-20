/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "SwitchThumbMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "SwitchThumbPart.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSSwitchSPI.h>

namespace WebCore {

SwitchThumbMac::SwitchThumbMac(SwitchThumbPart& owningPart, ControlFactoryMac& controlFactory, NSSwitch *switchPtr)
    : ControlMac(owningPart, controlFactory)
    , m_switchPtr(switchPtr)
{
    ASSERT(m_owningPart.type() == StyleAppearance::SwitchThumb);
}

void SwitchThumbMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    bool isOn = style.states.contains(ControlStyle::State::Checked);
    bool isEnabled = style.states.contains(ControlStyle::State::Enabled);
    bool isPressed = style.states.contains(ControlStyle::State::Pressed);

    auto inflatedRect = borderRect.rect();
    inflatedRect.inflateY(-1 / 2.f);

    auto trackRect = [m_switchPtr _trackFrameInRect:inflatedRect];

    trackRect.origin.x -= inflatedRect.x();
    trackRect.origin.y -= inflatedRect.y();

    auto knobRect = [m_switchPtr _knobFrameInTrackFrame:trackRect thumbIsOnRight:isOn];
    auto imageBuffer = context.createImageBuffer(inflatedRect.size(), deviceScaleFactor);

    if (!imageBuffer)
        return;

    auto cgContext = imageBuffer->context().platformContext();

    GraphicsContextStateSaver stateSaver(context);

    [[NSAppearance currentDrawingAppearance] _drawInRect:knobRect context:cgContext options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchKnob,
        (__bridge NSString *)kCUIStateKey: (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : isPressed ? kCUIStatePressed : kCUIStateActive),
        (__bridge NSString *)kCUISizeKey: (__bridge NSString *)kCUISizeRegular,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    if (style.states.contains(ControlStyle::State::RightToLeft)) {
        context.translate(2 * inflatedRect.x() + inflatedRect.width(), 0);
        context.scale(FloatSize(-1, 1));
    }

    context.drawConsumingImageBuffer(WTFMove(imageBuffer), inflatedRect.location());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
