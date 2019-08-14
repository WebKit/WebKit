/*
 * Copyright (C) 2006-2018 Apple Inc.  All rights reserved.
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
#include "Image.h"

#include "BitmapImage.h"
#include "BitmapInfo.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "NotImplemented.h"
#include <d2d1.h>
#include <windows.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

RefPtr<BitmapImage> BitmapImage::create(HBITMAP hBitmap)
{
    DIBSECTION dibSection;
    if (!GetObject(hBitmap, sizeof(DIBSECTION), &dibSection))
        return nullptr;

    ASSERT(dibSection.dsBm.bmBitsPixel == 32);
    if (dibSection.dsBm.bmBitsPixel != 32)
        return nullptr;

    ASSERT(dibSection.dsBm.bmBits);
    if (!dibSection.dsBm.bmBits)
        return nullptr;

    notImplemented();

    return nullptr;
}

bool BitmapImage::getHBITMAPOfSize(HBITMAP bmp, const IntSize* size)
{
    ASSERT(bmp);

    BITMAP bmpInfo;
    GetObject(bmp, sizeof(BITMAP), &bmpInfo);

    ASSERT(bmpInfo.bmBitsPixel == 32);
    int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
    
    notImplemented();

    return true;
}

void BitmapImage::drawFrameMatchingSourceSize(GraphicsContext& ctxt, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator compositeOp)
{
    size_t frames = frameCount();
    for (size_t i = 0; i < frames; ++i) {
        auto image = frameImageAtIndex(i).get();
        auto imageSize = nativeImageSize(image);
        if (image && clampTo<int>(imageSize.height()) == static_cast<int>(srcSize.height()) && clampTo<int>(imageSize.width()) == static_cast<int>(srcSize.width())) {
            size_t currentFrame = m_currentFrame;
            m_currentFrame = i;
            draw(ctxt, dstRect, FloatRect(0.0f, 0.0f, srcSize.width(), srcSize.height()), compositeOp, BlendMode::Normal, DecodingMode::Synchronous, ImageOrientation::None);
            m_currentFrame = currentFrame;
            return;
        }
    }

    // No image of the correct size was found, fallback to drawing the current frame
    FloatSize imageSize = BitmapImage::size();
    draw(ctxt, dstRect, FloatRect(0.0f, 0.0f, imageSize.width(), imageSize.height()), compositeOp, BlendMode::Normal, DecodingMode::Synchronous, ImageOrientation::None);
}

} // namespace WebCore
