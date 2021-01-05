/*
 * Copyright (C) 2014 Haiku, Inc.
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

#include "config.h"
#include "ShareableBitmap.h"

#include "BitmapImage.h"
#include "NotImplemented.h"
#include <WebCore/GraphicsContext.h>

#include <Bitmap.h>
#include <View.h>

using namespace WebCore;

namespace WebKit {

static inline RefPtr<StillImage> createSurfaceFromData(void* data, const WebCore::IntSize& size)
{
	BitmapRef* bitmap = new BitmapRef(BRect(B_ORIGIN, size), B_RGBA32, false);

    bitmap->ImportBits(static_cast<unsigned char*>(data),
		size.width() * size.height() * 4, 32 * size.height(), 0, B_RGB32);
    RefPtr<StillImage> image = StillImage::create(adoptRef(bitmap));
    return image;
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    RefPtr<StillImage> image = createBitmapSurface();
    BView* v = new BView(image->nativeImageForCurrentFrame(nullptr)->Bounds(),
        "Shareable", 0, 0);
    image->nativeImageForCurrentFrame(nullptr)->AddChild(v);
    return std::make_unique<GraphicsContext>(v);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    FloatRect destRect(dstPoint, srcRect.size());
    FloatRect srcRectScaled(srcRect);
    srcRectScaled.scale(scaleFactor);
    notImplemented();
    //context.platformContext()->DrawBitmap(surface.get(), destRect, srcRectScaled);
}

RefPtr<StillImage> ShareableBitmap::createBitmapSurface()
{
    RefPtr<StillImage> image = createSurfaceFromData(data(), m_size);

    ref(); // Balanced by deref in releaseSurfaceData.
    return image;
}

RefPtr<Image> ShareableBitmap::createImage()
{
    RefPtr<StillImage> surface = createBitmapSurface();
    if (!surface)
        return 0;

    return BitmapImage::create(surface->nativeImageForCurrentFrame(nullptr));
}

Checked<unsigned, RecordOverflow> ShareableBitmap::calculateBytesPerRow(WebCore::IntSize, const ShareableBitmap::Configuration&)
{
	notImplemented();
}

}
