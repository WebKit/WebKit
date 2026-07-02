/*
 *  Copyright (C) 2010,2017 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DragImage.h"

#include "Element.h"
#include "Image.h"
#include "NativeImage.h"
#include "TextFlags.h"
#include "TextIndicator.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkImageInfo.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/URL.h>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
    if (image)
        return { image->width(), image->height() };

    return { 0, 0 };
}

void deleteDragImage(DragImageRef)
{
    // Since this is a RefPtr, there's nothing additional we need to do to
    // delete it. It will be released when it falls out of scope.
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image)
        return nullptr;

    IntSize imageSize = dragImageSize(image);
    IntSize scaledSize(imageSize);
    scaledSize.scale(scale.width(), scale.height());
    if (imageSize == scaledSize)
        return image;

    auto imageInfo = SkImageInfo::Make(scaledSize.width(), scaledSize.height(), image->imageInfo().colorType(), image->imageInfo().alphaType());
    SkBitmap bitmap;
    bitmap.allocPixels(imageInfo);

    SkPixmap pixmap;
    if (!bitmap.peekPixels(&pixmap))
        return nullptr;

    if (!image->scalePixels(pixmap, SkSamplingOptions(SkCubicResampler::CatmullRom())))
        return nullptr;

    return SkImages::RasterFromBitmap(bitmap);
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float fraction)
{
    if (!image)
        return nullptr;

    SkBitmap bitmap;
    bitmap.allocPixels(image->imageInfo());

    SkPixmap pixmap;
    if (!bitmap.peekPixels(&pixmap))
        return nullptr;

    auto canvas = SkCanvas::MakeRasterDirect(bitmap.info(), pixmap.writable_addr(), bitmap.rowBytes());

    SkPaint paint;
    paint.setAlphaf(fraction);

    canvas->drawImage(image, 0, 0,  { }, &paint);

    return SkImages::RasterFromBitmap(bitmap);
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation, GraphicsClient*, float)
{
    return image->currentNativeImage()->platformImage();
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    return nullptr;
}

DragImageData createDragImageForLink(Element&, URL&, const String&, float)
{
    return { nullptr, nullptr };
}

DragImageRef createDragImageForColor(const Color&, const FloatRect&, float, Path&)
{
    return nullptr;
}

}
