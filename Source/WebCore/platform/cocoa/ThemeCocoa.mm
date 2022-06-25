/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#import "ThemeCocoa.h"

#import "ApplePayLogoSystemImage.h"
#import "FontCascade.h"
#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import "ImageBuffer.h"
#import <dlfcn.h>

namespace WebCore {

void ThemeCocoa::drawNamedImage(const String& name, GraphicsContext& context, const FloatSize& size) const
{
    if (name == "wireless-playback"_s) {
        GraphicsContextStateSaver stateSaver(context);
        context.setFillColor(Color::black);

        FloatSize wirelessPlaybackSrcSize(32, 24.016);
        auto largestRect = largestRectWithAspectRatioInsideRect(wirelessPlaybackSrcSize.aspectRatio(), FloatRect(FloatPoint::zero(), size));
        context.translate(largestRect.x(), largestRect.y());
        context.scale(largestRect.width() / wirelessPlaybackSrcSize.width());

        Path outline;
        outline.moveTo(FloatPoint(24.066, 18));
        outline.addLineTo(FloatPoint(22.111, 16));
        outline.addLineTo(FloatPoint(30, 16));
        outline.addLineTo(FloatPoint(30, 2));
        outline.addLineTo(FloatPoint(2, 2));
        outline.addLineTo(FloatPoint(2, 16));
        outline.addLineTo(FloatPoint(9.908, 16));
        outline.addLineTo(FloatPoint(7.953, 18));
        outline.addLineTo(FloatPoint(0, 18));
        outline.addLineTo(FloatPoint(0, 0));
        outline.addLineTo(FloatPoint(32, 0));
        outline.addLineTo(FloatPoint(32, 18));
        outline.addLineTo(FloatPoint(24.066, 18));
        outline.closeSubpath();
        outline.moveTo(FloatPoint(26.917, 24.016));
        outline.addLineTo(FloatPoint(5.040, 24.016));
        outline.addLineTo(FloatPoint(15.978, 12.828));
        outline.addLineTo(FloatPoint(26.917, 24.016));
        outline.closeSubpath();

        context.fillPath(outline);
        return;
    }

#if ENABLE(APPLE_PAY)
    if (name == "apple-pay-logo-black"_s) {
        context.drawSystemImage(ApplePayLogoSystemImage::create(ApplePayLogoStyle::Black), FloatRect(FloatPoint::zero(), size));
        return;
    }

    if (name == "apple-pay-logo-white"_s) {
        context.drawSystemImage(ApplePayLogoSystemImage::create(ApplePayLogoStyle::White), FloatRect(FloatPoint::zero(), size));
        return;
    }
#endif

    Theme::drawNamedImage(name, context, size);
}

}
