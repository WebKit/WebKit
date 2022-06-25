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

#if PLATFORM(IOS_FAMILY)

#import "BitmapImage.h"
#import "GraphicsContext.h"

namespace WebCore {
    
Icon::Icon(RefPtr<NativeImage>&& image)
    : m_cgImage(WTFMove(image))
{
    ASSERT(m_cgImage);
}

Icon::~Icon()
{
}

RefPtr<Icon> Icon::createIconForFiles(const Vector<String>& /*filenames*/)
{
    return nullptr;
}

RefPtr<Icon> Icon::createIconForImage(PlatformImagePtr&& image)
{
    if (!image)
        return nullptr;

    return adoptRef(new Icon(NativeImage::create(WTFMove(image))));
}

void Icon::paint(GraphicsContext& context, const FloatRect& destRect)
{
    if (context.paintingDisabled())
        return;

    GraphicsContextStateSaver stateSaver(context);

    FloatRect srcRect(FloatPoint::zero(), m_cgImage->size());
    context.setImageInterpolationQuality(InterpolationQuality::High);
    context.drawNativeImage(*m_cgImage, srcRect.size(), destRect, srcRect);
}

}

#endif // PLATFORM(IOS_FAMILY)
