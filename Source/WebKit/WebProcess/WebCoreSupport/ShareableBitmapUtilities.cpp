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
#include <WebCore/FrameSnapshotting.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/IntSize.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RenderElementInlines.h>
#include <WebCore/RenderImage.h>
#include <WebCore/RenderVideo.h>

namespace WebKit {
using namespace WebCore;

RefPtr<ShareableBitmap> createShareableBitmap(RenderImage& renderImage, CreateShareableBitmapFromImageOptions&& options)
{
    Ref frame = renderImage.frame();
    auto* localMainFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localMainFrame)
        return { };

    auto colorSpaceForBitmap = screenColorSpace(localMainFrame->view());
    if (!renderImage.isMedia() && !renderImage.opacity() && options.useSnapshotForTransparentImages == UseSnapshotForTransparentImages::Yes) {
        auto snapshotRect = renderImage.absoluteBoundingBoxRect();
        if (snapshotRect.isEmpty())
            return { };

        OptionSet<SnapshotFlags> snapshotFlags { SnapshotFlags::ExcludeSelectionHighlighting, SnapshotFlags::PaintEverythingExcludingSelection };
        auto imageBuffer = snapshotFrameRect(frame.get(), snapshotRect, { snapshotFlags, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
        if (!imageBuffer)
            return { };

        auto snapshotImage = ImageBuffer::sinkIntoImage(WTFMove(imageBuffer), PreserveResolution::Yes);
        if (!snapshotImage)
            return { };

        auto bitmap = ShareableBitmap::create({ snapshotRect.size(), WTFMove(colorSpaceForBitmap) });
        if (!bitmap)
            return { };

        auto context = bitmap->createGraphicsContext();
        if (!context)
            return { };

        context->drawImage(*snapshotImage, { FloatPoint::zero(), snapshotRect.size() });
        return bitmap;
    }

#if ENABLE(VIDEO)
    if (is<RenderVideo>(renderImage)) {
        auto& renderVideo = downcast<RenderVideo>(renderImage);
        Ref video = renderVideo.videoElement();
        auto image = video->nativeImageForCurrentTime();
        if (!image)
            return { };

        auto imageSize = image->size();
        if (imageSize.isEmpty() || imageSize.width() <= 1 || imageSize.height() <= 1)
            return { };

        auto bitmap = ShareableBitmap::create({ imageSize, WTFMove(colorSpaceForBitmap) });
        if (!bitmap)
            return { };

        auto context = bitmap->createGraphicsContext();
        if (!context)
            return { };

        context->drawNativeImage(*image, imageSize, FloatRect { { }, imageSize }, FloatRect { { }, imageSize });
        return bitmap;
    }
#endif // ENABLE(VIDEO)

    auto* cachedImage = renderImage.cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return { };

    auto* image = cachedImage->imageForRenderer(&renderImage);
    if (!image || image->width() <= 1 || image->height() <= 1)
        return { };

    if (options.allowAnimatedImages == AllowAnimatedImages::No && image->isAnimated())
        return { };

    auto bitmapSize = cachedImage->imageSizeForRenderer(&renderImage);
    if (options.screenSizeInPixels) {
        auto scaledSize = largestRectWithAspectRatioInsideRect(bitmapSize.width() / bitmapSize.height(), { FloatPoint(), *options.screenSizeInPixels }).size();
        bitmapSize = scaledSize.width() < bitmapSize.width() ? scaledSize : bitmapSize;
    }

    // FIXME: Only select ExtendedColor on images known to need wide gamut.
    auto sharedBitmap = ShareableBitmap::create({ IntSize(bitmapSize), WTFMove(colorSpaceForBitmap) });
    if (!sharedBitmap)
        return { };

    auto graphicsContext = sharedBitmap->createGraphicsContext();
    if (!graphicsContext)
        return { };

    graphicsContext->drawImage(*image, FloatRect(0, 0, bitmapSize.width(), bitmapSize.height()), { renderImage.imageOrientation() });
    return sharedBitmap;
}

}
