/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DragImage.h"

#include "ImageBuffer.h"
#include "NativeImage.h"
#include "NotImplemented.h"

namespace WebCore {

DragImageRef fitDragImageToMaxSize(DragImageRef image, const IntSize& layoutSize, const IntSize& maxSize)
{
    float heightResizeRatio = 0.0f;
    float widthResizeRatio = 0.0f;
    float resizeRatio = -1.0f;
    IntSize originalSize = dragImageSize(image);

    if (layoutSize.width() > maxSize.width()) {
        widthResizeRatio = maxSize.width() / (float)layoutSize.width();
        resizeRatio = widthResizeRatio;
    }

    if (layoutSize.height() > maxSize.height()) {
        heightResizeRatio = maxSize.height() / (float)layoutSize.height();
        if ((resizeRatio < 0.0f) || (resizeRatio > heightResizeRatio))
            resizeRatio = heightResizeRatio;
    }

    if (layoutSize == originalSize)
        return resizeRatio > 0.0f ? scaleDragImage(image, FloatSize(resizeRatio, resizeRatio)) : image;

    // The image was scaled in the webpage so at minimum we must account for that scaling.
    float scaleX = layoutSize.width() / (float)originalSize.width();
    float scaleY = layoutSize.height() / (float)originalSize.height();
    if (resizeRatio > 0.0f) {
        scaleX *= resizeRatio;
        scaleY *= resizeRatio;
    }

    return scaleDragImage(image, FloatSize(scaleX, scaleY));
}

#if !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)
DragImageRef platformAdjustDragImageForDeviceScaleFactor(DragImageRef image, float deviceScaleFactor)
{
    // Later code expects the drag image to be scaled by device's scale factor.
    return scaleDragImage(image, { deviceScaleFactor, deviceScaleFactor });
}
#endif

#if !PLATFORM(MAC)

const int linkDragBorderInset = 2;

IntPoint dragOffsetForLinkDragImage(DragImageRef dragImage)
{
    IntSize size = dragImageSize(dragImage);
    return { -size.width() / 2, -linkDragBorderInset };
}

FloatPoint anchorPointForLinkDragImage(DragImageRef dragImage)
{
    IntSize size = dragImageSize(dragImage);
    return { 0.5, static_cast<float>((size.height() - linkDragBorderInset) / size.height()) };
}

IntPoint platformAdjustDragImageHotSpot(const IntPoint& hotSpot, const IntSize&)
{
    return hotSpot;
}

#endif

DragImage::DragImage()
    : m_dragImageRef { nullptr }
{
}

DragImage::DragImage(DragImageRef dragImageRef)
    : m_dragImageRef { dragImageRef }
{
}

DragImage::DragImage(DragImage&& other)
    : m_dragImageRef { std::exchange(other.m_dragImageRef, nullptr) }
    , m_textIndicator { WTF::move(other.m_textIndicator) }
    , m_visiblePath { WTF::move(other.m_visiblePath) }
{
}

DragImage& DragImage::operator=(DragImage&& other)
{
    if (m_dragImageRef)
        deleteDragImage(m_dragImageRef);

    m_dragImageRef = std::exchange(other.m_dragImageRef, nullptr);
    m_textIndicator = WTF::move(other.m_textIndicator);
    m_visiblePath = WTF::move(other.m_visiblePath);

    return *this;
}

DragImage::~DragImage()
{
    if (m_dragImageRef)
        deleteDragImage(m_dragImageRef);
}

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN) && !(PLATFORM(WPE) && ENABLE(DRAG_SUPPORT))

IntSize dragImageSize(DragImageRef)
{
    notImplemented();
    return { 0, 0 };
}

void deleteDragImage(DragImageRef)
{
    notImplemented();
}

DragImageRef scaleDragImage(DragImageRef, FloatSize)
{
    notImplemented();
    return nullptr;
}

DragImageRef dissolveDragImageToFraction(DragImageRef, float)
{
    notImplemented();
    return nullptr;
}

DragImageRef createDragImageForColor(const Color&, const FloatRect&, float, Path&)
{
    notImplemented();
    return nullptr;
}

DragImageRef createDragImageFromImage(NativeImage&, GraphicsClient*, float)
{
    notImplemented();
    return nullptr;
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    notImplemented();
    return nullptr;
}

#endif

} // namespace WebCore

