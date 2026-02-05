/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include <WebCore/CachedImage.h>
#include <WebCore/FrameSnapshotting.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/IntSize.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RenderElementStyleInlines.h>
#include <WebCore/RenderImage.h>
#include <WebCore/RenderObjectInlines.h>
#include <WebCore/RenderVideo.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/NativePromise.h>

namespace WebKit {
using namespace WebCore;

RefPtr<ShareableBitmap> createShareableBitmap(RenderImage& renderImage, CreateShareableBitmapFromImageOptions&& options)
{
    Ref frame = renderImage.frame();
    auto colorSpaceForBitmap = screenColorSpace(frame->protectedMainFrame()->protectedVirtualView().get());
    if (!renderImage.isRenderMedia() && !renderImage.opacity() && options.useSnapshotForTransparentImages == UseSnapshotForTransparentImages::Yes) {
        auto snapshotRect = renderImage.absoluteBoundingBoxRect();
        if (snapshotRect.isEmpty())
            return { };

        OptionSet<SnapshotFlags> snapshotFlags { SnapshotFlags::ExcludeSelectionHighlighting, SnapshotFlags::PaintEverythingExcludingSelection };
        auto imageBuffer = snapshotFrameRect(frame.get(), snapshotRect, { snapshotFlags, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
        if (!imageBuffer)
            return { };

        auto snapshotImage = ImageBuffer::sinkIntoNativeImage(WTF::move(imageBuffer));
        if (!snapshotImage)
            return { };

        auto bitmap = ShareableBitmap::create({ snapshotImage->size(), WTF::move(colorSpaceForBitmap) });
        if (!bitmap)
            return { };

        auto context = bitmap->createGraphicsContext();
        if (!context)
            return { };
        FloatRect imageRect { { }, snapshotImage->size() };
        context->drawNativeImage(*snapshotImage, imageRect, imageRect);
        return bitmap;
    }

#if ENABLE(VIDEO)
    if (auto* renderVideo = dynamicDowncast<RenderVideo>(renderImage))
        return renderVideo->protectedVideoElement()->bitmapImageForCurrentTimeSync();
#endif // ENABLE(VIDEO)

    auto* cachedImage = renderImage.cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return { };

    RefPtr image = cachedImage->imageForRenderer(&renderImage);
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
    auto sharedBitmap = ShareableBitmap::create({ IntSize(bitmapSize), WTF::move(colorSpaceForBitmap) });
    if (!sharedBitmap)
        return { };

    auto graphicsContext = sharedBitmap->createGraphicsContext();
    if (!graphicsContext)
        return { };

    graphicsContext->drawImage(*image, FloatRect(0, 0, bitmapSize.width(), bitmapSize.height()), { renderImage.imageOrientation() });
    return sharedBitmap;
}

Ref<NativePromise<Ref<WebCore::ShareableBitmap>, void>> createShareableBitmapAsync(WebCore::RenderImage& renderImage, CreateShareableBitmapFromImageOptions&& options)
{
    if (auto* renderVideo = dynamicDowncast<RenderVideo>(renderImage))
        return renderVideo->protectedVideoElement()->bitmapImageForCurrentTime();
    if (RefPtr shareableBitmap = createShareableBitmap(renderImage, WTF::move(options)))
        return NativePromise<Ref<WebCore::ShareableBitmap>, void>::createAndResolve(shareableBitmap.releaseNonNull());
    return NativePromise<Ref<WebCore::ShareableBitmap>, void>::createAndReject();
}

}
