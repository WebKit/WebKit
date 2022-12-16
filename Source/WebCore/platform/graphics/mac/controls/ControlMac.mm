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
#import "ControlMac.h"

#if PLATFORM(MAC)

#import "ColorCocoa.h"
#import "GraphicsContextCG.h"
#import "LocalCurrentGraphicsContext.h"
#import "WebControlView.h"
#import <pal/spi/cocoa/NSButtonCellSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>

namespace WebCore {

ControlMac::ControlMac(const ControlPart& owningPart, ControlFactoryMac& controlFactory)
    : PlatformControl(owningPart)
    , m_controlFactory(controlFactory)
{
}

void ControlMac::setFocusRingClipRect(const FloatRect& clipBounds)
{
    [WebControlView setClipBounds:clipBounds];
}

void ControlMac::updateCellStates(const FloatRect&, const ControlStyle& style)
{
    [WebControlWindow setHasKeyAppearance:!style.states.contains(ControlStyle::State::WindowInactive)];
}

static void drawFocusRing(NSRect cellFrame, NSCell *cell, NSView *view)
{
    CGContextRef cgContext = [[NSGraphicsContext currentContext] CGContext];

    CGContextStateSaver stateSaver(cgContext);

    CGFocusRingStyle focusRingStyle;
    NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, std::numeric_limits<double>::max());

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;

    // FIXME: This color should be shared with RenderThemeMac. For now just use the same NSColor color.
    // The color is expected to be opaque, since CoreGraphics will apply opacity when drawing (because opacity is normally animated).
    auto color = colorFromCocoaColor([NSColor keyboardFocusIndicatorColor]).opaqueColor();
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color).get()));
    CGContextSetStyle(cgContext, style.get());

    CGContextBeginTransparencyLayerWithRect(cgContext, NSRectToCGRect(cellFrame), nullptr);
    [cell drawFocusRingMaskWithFrame:cellFrame inView:view];
    CGContextEndTransparencyLayer(cgContext);
}

static void drawCellOrFocusRing(GraphicsContext& context, const FloatRect& rect, const ControlStyle& style, NSCell *cell, NSView *view, bool drawCell)
{
    LocalCurrentGraphicsContext localContext(context);

    if (drawCell) {
        if ([cell isKindOfClass:[NSSliderCell class]]) {
            // For slider cells, draw only the knob.
            [(NSSliderCell *)cell drawKnob:rect];
        } else
            [cell drawWithFrame:rect inView:view];
    }

    if (style.states.contains(ControlStyle::State::Focused))
        drawFocusRing(rect, cell, view);

    [cell setControlView:nil];
}

void ControlMac::draw(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style, NSCell *cell, NSView *view, bool drawCell)
{
    auto platformContext = context.platformContext();
    auto userCTM = AffineTransform(CGAffineTransformConcat(CGContextGetCTM(platformContext), CGAffineTransformInvert(CGContextGetBaseCTM(platformContext))));
    bool useImageBuffer = userCTM.xScale() != 1.0 || userCTM.yScale() != 1.0;

    if (!useImageBuffer) {
        drawCellOrFocusRing(context, rect, style, cell, view, drawCell);
        return;
    }

    auto imageBufferRect = FloatRect { { 0, 0 }, rect.size() };

    auto imageBuffer = context.createImageBuffer(rect.size(), deviceScaleFactor);
    if (!imageBuffer)
        return;

    drawCellOrFocusRing(imageBuffer->context(), imageBufferRect, style, cell, view, drawCell);
    context.drawConsumingImageBuffer(WTFMove(imageBuffer), rect.location());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
