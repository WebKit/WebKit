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

DragImage::DragImage(DragImage&& other)
    : m_platformImage(other.takePlatformImage())
{
}

DragImage& DragImage::operator=(DragImage&& other)
{
    if (m_platformImage)
        deletePlatformImage(takePlatformImage());
    m_platformImage = other.takePlatformImage();
    return *this;
}

DragImage::~DragImage()
{
    if (m_platformImage)
        deletePlatformImage(takePlatformImage());
}

void DragImage::fitToMaxSize(const IntSize& layoutSize, const IntSize& maxSize)
{
    float heightResizeRatio = 0.0f;
    float widthResizeRatio = 0.0f;
    float resizeRatio = -1.0f;
    IntSize originalSize = size();

    if (layoutSize.width() > maxSize.width()) {
        widthResizeRatio = maxSize.width() / (float)layoutSize.width();
        resizeRatio = widthResizeRatio;
    }

    if (layoutSize.height() > maxSize.height()) {
        heightResizeRatio = maxSize.height() / (float)layoutSize.height();
        if ((resizeRatio < 0.0f) || (resizeRatio > heightResizeRatio))
            resizeRatio = heightResizeRatio;
    }

    if (layoutSize == originalSize) {
        if (resizeRatio > 0.0f)
            scale(FloatSize(resizeRatio, resizeRatio));
        return;
    }

    // The image was scaled in the webpage so at minimum we must account for that scaling.
    float scaleX = layoutSize.width() / (float)originalSize.width();
    float scaleY = layoutSize.height() / (float)originalSize.height();
    if (resizeRatio > 0.0f) {
        scaleX *= resizeRatio;
        scaleY *= resizeRatio;
    }

    scale(FloatSize(scaleX, scaleY));
}

#if !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)
void DragImage::adjustForDeviceScaleFactor(float deviceScaleFactor)
{
    scale({ deviceScaleFactor, deviceScaleFactor });
}
#endif

#if !PLATFORM(MAC)

constexpr int linkDragBorderInset = 2;

IntPoint DragImage::dragOffsetForLink() const
{
    IntSize size = this->size();
    return { -size.width() / 2, -linkDragBorderInset };
}

FloatPoint DragImage::anchorPointForLink() const
{
    IntSize size = this->size();
    return { 0.5, static_cast<float>((size.height() - linkDragBorderInset) / size.height()) };
}

IntPoint DragImage::platformAdjustDragImageHotSpot(const IntPoint& hotSpot, const IntSize&)
{
    return hotSpot;
}

#endif

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN) && !(PLATFORM(WPE) && ENABLE(DRAG_SUPPORT))

IntSize DragImage::size() const
{
    notImplemented();
    return { 0, 0 };
}

void DragImage::deletePlatformImage(PlatformDragImage)
{
    notImplemented();
}

void DragImage::scale(FloatSize)
{
    notImplemented();
    *this = { };
}

void DragImage::dissolveToFraction(float)
{
    notImplemented();
    *this = { };
}

DragImage DragImage::createForColor(const Color&, const FloatRect&, float, Path&)
{
    notImplemented();
    return { };
}

DragImage DragImage::create(NativeImage&, GraphicsClient*, float)
{
    notImplemented();
    return { };
}

DragImage DragImage::createIconForCachedImageFilename(const String&)
{
    notImplemented();
    return { };
}

#endif

} // namespace WebCore
