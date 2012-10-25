/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DragImage.h"

#include "AffineTransform.h"
#include "BitmapImage.h"
#include "FloatRect.h"
#include "Image.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkMatrix.h"

#include "skia/ext/image_operations.h"

#include <wtf/RefPtr.h>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
    if (!image)
        return IntSize();

    return IntSize(image->bitmap->width(), image->bitmap->height());
}

void deleteDragImage(DragImageRef image)
{
    if (image)
        delete image->bitmap;
    delete image;
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image)
        return 0;

    int imageWidth = scale.width() * image->bitmap->width();
    int imageHeight = scale.height() * image->bitmap->height();
    SkBitmap* scaledImage = new SkBitmap(
        skia::ImageOperations::Resize(*image->bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
                                      imageWidth, imageHeight));
    delete image->bitmap;
    image->bitmap = scaledImage;
    return image;
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float fraction)
{
    if (!image)
        return 0;

    image->bitmap->setIsOpaque(false);
    image->bitmap->lockPixels();

    for (int row = 0; row < image->bitmap->height(); ++row) {
        for (int column = 0; column < image->bitmap->width(); ++column) {
            uint32_t* pixel = image->bitmap->getAddr32(column, row);
            *pixel = SkPreMultiplyARGB(SkColorGetA(*pixel) * fraction,
                                       SkColorGetR(*pixel),
                                       SkColorGetG(*pixel),
                                       SkColorGetB(*pixel));
        }
    }

    image->bitmap->unlockPixels();

    return image;
}

DragImageRef createDragImageFromImage(Image* image, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (!image)
        return 0;

    NativeImageSkia* bitmap = image->nativeImageForCurrentFrame();
    if (!bitmap)
        return 0;

    DragImageChromium* dragImageChromium = new DragImageChromium;
    dragImageChromium->bitmap = new SkBitmap();
    dragImageChromium->resolutionScale = bitmap->resolutionScale();

    if (image->isBitmapImage()) {
        ImageOrientation orientation = DefaultImageOrientation;
        BitmapImage* bitmapImage = static_cast<BitmapImage*>(image);
        IntSize sizeRespectingOrientation = bitmapImage->sizeRespectingOrientation();

        if (shouldRespectImageOrientation == RespectImageOrientation)
            orientation = bitmapImage->currentFrameOrientation();

        if (orientation != DefaultImageOrientation) {
            // Construct a correctly-rotated copy of the image to use as the drag image.
            dragImageChromium->bitmap->setConfig(
                SkBitmap::kARGB_8888_Config, sizeRespectingOrientation.width(), sizeRespectingOrientation.height());
            dragImageChromium->bitmap->allocPixels();

            FloatRect destRect(FloatPoint(), sizeRespectingOrientation);
            SkCanvas canvas(*dragImageChromium->bitmap);

            // ImageOrientation expects the origin in the lower left corner, so flip, transform, and then...
            canvas.translate(0, destRect.height());
            canvas.scale(1, -1);
            canvas.concat(orientation.transformFromDefault(sizeRespectingOrientation));

            if (orientation.usesWidthAsHeight())
                destRect = FloatRect(destRect.x(), destRect.y(), destRect.height(), destRect.width());

            // ...flip back.
            canvas.translate(0, destRect.height());
            canvas.scale(1, -1);

            canvas.drawBitmapRect(bitmap->bitmap(), 0, destRect);
            return dragImageChromium;
        }
    }

    bitmap->bitmap().copyTo(dragImageChromium->bitmap, SkBitmap::kARGB_8888_Config);
    return dragImageChromium;
}

DragImageRef createDragImageIconForCachedImage(CachedImage*)
{
    notImplemented();
    return 0;
}

} // namespace WebCore
