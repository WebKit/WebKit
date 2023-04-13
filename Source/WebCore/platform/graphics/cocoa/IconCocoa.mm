/*
 * Copyright (C) 2007, 2008, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Icon.h"

#if PLATFORM(COCOA)

#import "BitmapImage.h"
#import "GraphicsContext.h"
#import <pal/ios/UIKitSoftLink.h>

namespace WebCore {

Icon::Icon(CocoaImage *image)
    : m_image(image)
{
}

Icon::~Icon()
{
}

RefPtr<Icon> Icon::create(CocoaImage *image)
{
    if (!image)
        return nullptr;

    return adoptRef(new Icon(image));
}

RefPtr<Icon> Icon::create(PlatformImagePtr&& platformImage)
{
    if (!platformImage)
        return nullptr;

#if USE(APPKIT)
    auto image = adoptNS([[NSImage alloc] initWithCGImage:platformImage.get() size:NSZeroSize]);
#else
    auto image = adoptNS([PAL::allocUIImageInstance() initWithCGImage:platformImage.get()]);
#endif
    return adoptRef(new Icon(image.get()));
}

void Icon::paint(GraphicsContext& context, const FloatRect& destRect)
{
    if (context.paintingDisabled())
        return;

    GraphicsContextStateSaver stateSaver(context);

#if USE(APPKIT)
    auto cgImage = [m_image CGImageForProposedRect:nil context:nil hints:nil];
#else
    auto cgImage = [m_image CGImage];
#endif
    auto image = NativeImage::create(cgImage);

    FloatRect srcRect(FloatPoint::zero(), image->size());
    context.setImageInterpolationQuality(InterpolationQuality::High);
    context.drawNativeImage(*image, srcRect.size(), destRect, srcRect);
}

}

#endif // PLATFORM(COCOA)
