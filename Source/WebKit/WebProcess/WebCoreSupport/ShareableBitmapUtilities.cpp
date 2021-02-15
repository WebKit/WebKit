/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ShareableBitmapUtilities.h"

#include "ShareableBitmap.h"
#include <WebCore/CachedImage.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RenderImage.h>

namespace WebKit {
using namespace WebCore;

RefPtr<ShareableBitmap> createShareableBitmap(RenderImage& renderImage, Optional<FloatSize> screenSizeInPixels)
{
    auto* cachedImage = renderImage.cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return nullptr;

    auto* image = cachedImage->imageForRenderer(&renderImage);
    if (!image || image->width() <= 1 || image->height() <= 1)
        return nullptr;

    auto bitmapSize = cachedImage->imageSizeForRenderer(&renderImage);
    if (screenSizeInPixels) {
        auto scaledSize = largestRectWithAspectRatioInsideRect(bitmapSize.width() / bitmapSize.height(), { FloatPoint(), *screenSizeInPixels }).size();
        bitmapSize = scaledSize.width() < bitmapSize.width() ? scaledSize : bitmapSize;
    }

    // FIXME: Only select ExtendedColor on images known to need wide gamut.
    ShareableBitmap::Configuration bitmapConfiguration;
#if USE(CG)
    bitmapConfiguration.colorSpace.cgColorSpace = screenColorSpace(renderImage.frame().mainFrame().view());
#endif

    auto sharedBitmap = ShareableBitmap::createShareable(IntSize(bitmapSize), bitmapConfiguration);
    if (!sharedBitmap)
        return nullptr;

    auto graphicsContext = sharedBitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    graphicsContext->drawImage(*image, FloatRect(0, 0, bitmapSize.width(), bitmapSize.height()), { renderImage.imageOrientation() });
    return sharedBitmap;
}

}
