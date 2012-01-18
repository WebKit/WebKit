/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageDecoder.h"

#include "NotImplemented.h"

#if PLATFORM(CHROMIUM) && OS(DARWIN)
#include "GraphicsContextCG.h"
#include "SkCGUtils.h"
#endif

namespace WebCore {

ImageFrame::ImageFrame()
    : m_status(FrameEmpty)
    , m_duration(0)
    , m_disposalMethod(DisposeNotSpecified)
    , m_premultiplyAlpha(true)
{
}

ImageFrame& ImageFrame::operator=(const ImageFrame& other)
{
    if (this == &other)
        return *this;

    m_bitmap = other.m_bitmap;
    // Keep the pixels locked since we will be writing directly into the
    // bitmap throughout this object's lifetime.
    m_bitmap.bitmap().lockPixels();
    setOriginalFrameRect(other.originalFrameRect());
    setStatus(other.status());
    setDuration(other.duration());
    setDisposalMethod(other.disposalMethod());
    setPremultiplyAlpha(other.premultiplyAlpha());
    return *this;
}

void ImageFrame::clearPixelData()
{
    m_bitmap.bitmap().reset();
    m_status = FrameEmpty;
    // NOTE: Do not reset other members here; clearFrameBufferCache()
    // calls this to free the bitmap data, but other functions like
    // initFrameBuffer() and frameComplete() may still need to read
    // other metadata out of this frame later.
}

void ImageFrame::zeroFillPixelData()
{
    m_bitmap.bitmap().eraseARGB(0, 0, 0, 0);
}

bool ImageFrame::copyBitmapData(const ImageFrame& other)
{
    if (this == &other)
        return true;

    m_bitmap.bitmap().reset();
    const NativeImageSkia& otherBitmap = other.m_bitmap;
    return otherBitmap.bitmap().copyTo(&m_bitmap.bitmap(), otherBitmap.bitmap().config());
}

bool ImageFrame::setSize(int newWidth, int newHeight)
{
    // setSize() should only be called once, it leaks memory otherwise.
    ASSERT(!width() && !height());

    m_bitmap.bitmap().setConfig(SkBitmap::kARGB_8888_Config, newWidth, newHeight);
    if (!m_bitmap.bitmap().allocPixels())
        return false;

    zeroFillPixelData();
    return true;
}

NativeImagePtr ImageFrame::asNewNativeImage() const
{
    return new NativeImageSkia(m_bitmap);
}

bool ImageFrame::hasAlpha() const
{
    return !m_bitmap.bitmap().isOpaque();
}

void ImageFrame::setHasAlpha(bool alpha)
{
    m_bitmap.bitmap().setIsOpaque(!alpha);
}

#if PLATFORM(CHROMIUM) && OS(DARWIN)
static void resolveColorSpace(const SkBitmap& bitmap, CGColorSpaceRef colorSpace)
{
    int width = bitmap.width();
    int height = bitmap.height();
    CGImageRef srcImage = SkCreateCGImageRefWithColorspace(bitmap, colorSpace);
    SkAutoLockPixels lock(bitmap);
    void* pixels = bitmap.getPixels();
    RetainPtr<CGContextRef> cgBitmap(AdoptCF, CGBitmapContextCreate(pixels, width, height, 8, width * 4, deviceRGBColorSpaceRef(), kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst));
    if (!cgBitmap)
        return;
    CGContextSetBlendMode(cgBitmap.get(), kCGBlendModeCopy);
    CGRect bounds = { {0, 0}, {width, height} };
    CGContextDrawImage(cgBitmap.get(), bounds, srcImage);
}

static CGColorSpaceRef createColorSpace(const ColorProfile& colorProfile)
{
    RetainPtr<CFDataRef> data(AdoptCF, CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(colorProfile.data()), colorProfile.size()));
#ifndef TARGETING_LEOPARD
    return CGColorSpaceCreateWithICCProfile(data.get());
#else
    RetainPtr<CGDataProviderRef> profileDataProvider(AdoptCF, CGDataProviderCreateWithCFData(data.get()));
    CGFloat ranges[] = {0.0, 255.0, 0.0, 255.0, 0.0, 255.0};
    return CGColorSpaceCreateICCBased(3, ranges, profileDataProvider.get(), deviceRGBColorSpaceRef());
#endif
}
#endif

void ImageFrame::setColorProfile(const ColorProfile& colorProfile)
{
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    m_colorProfile = colorProfile;
#else
    notImplemented();
#endif
}

void ImageFrame::setStatus(FrameStatus status)
{
    m_status = status;
    if (m_status == FrameComplete) {
        m_bitmap.setDataComplete();  // Tell the bitmap it's done.
#if PLATFORM(CHROMIUM) && OS(DARWIN)
        // resolveColorSpace() and callees assume that the alpha channel is
        // premultiplied, so don't apply the color profile if it isn't.
        if (m_colorProfile.isEmpty() || (!m_premultiplyAlpha && hasAlpha()))
            return;
        RetainPtr<CGColorSpaceRef> cgColorSpace(AdoptCF, createColorSpace(m_colorProfile));
        resolveColorSpace(m_bitmap.bitmap(), cgColorSpace.get());
#endif
    }
}

int ImageFrame::width() const
{
    return m_bitmap.bitmap().width();
}

int ImageFrame::height() const
{
    return m_bitmap.bitmap().height();
}

} // namespace WebCore
