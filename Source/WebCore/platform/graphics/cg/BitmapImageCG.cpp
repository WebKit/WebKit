/*
 * Copyright (C) 2004, 2005, 2006 Apple Inc. All rights reserved.
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
#include "BitmapImage.h"

#if USE(CG)

#include "FloatConversion.h"
#include "GraphicsContextCG.h"
#include "ImageObserver.h"
#include "SubimageCacheWithTimer.h"
#include <wtf/RetainPtr.h>

#if USE(APPKIT)
#include <ApplicationServices/ApplicationServices.h>
#endif

#if PLATFORM(IOS)
#include <CoreGraphics/CGContextPrivate.h>
#endif

#if PLATFORM(COCOA)
#include "WebCoreSystemInterface.h"
#endif

#if PLATFORM(WIN)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

namespace WebCore {

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    m_orientation = DefaultImageOrientation;

#if PLATFORM(IOS)
    m_frameBytes = 0;
    m_scale = 1;
    m_haveInfo = false;
#endif

    if (m_frame) {
#if CACHE_SUBIMAGES
        subimageCache().clearImage(m_frame);
#endif
        CGImageRelease(m_frame);
        m_frame = 0;
        return true;
    }
    return false;
}

BitmapImage::BitmapImage(CGImageRef cgImage, ImageObserver* observer)
    : Image(observer)
    , m_currentFrame(0)
    , m_frames(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_decodedSize(0)
    , m_decodedPropertiesSize(0)
    , m_frameCount(1)
    , m_isSolidColor(false)
    , m_checkedForSolidColor(false)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_haveFrameCount(true)
{
    CGFloat width = CGImageGetWidth(cgImage);
    CGFloat height = CGImageGetHeight(cgImage);
    m_decodedSize = width * height * 4;
    m_size = IntSize(width, height);

    // Since we don't have a decoder, we can't figure out the image orientation.
    // Set m_sizeRespectingOrientation to be the same as m_size so it's not 0x0.
    m_sizeRespectingOrientation = IntSize(width, height);

#if PLATFORM(IOS)
    m_originalSize = IntSize(width, height);
    m_originalSizeRespectingOrientation = IntSize(width, height);
#endif

    m_frames.grow(1);
    m_frames[0].m_frame = CGImageRetain(cgImage);
    m_frames[0].m_hasAlpha = true;
    m_frames[0].m_haveMetadata = true;

#if PLATFORM(IOS)
    m_frames[0].m_scale = 1;
#endif

    checkForSolidColor();
}

// Drawing Routines

void BitmapImage::checkForSolidColor()
{
    m_checkedForSolidColor = true;
    if (frameCount() > 1) {
        m_isSolidColor = false;
        return;
    }

#if !PLATFORM(IOS)
    CGImageRef image = frameAtIndex(0);
#else
    // Note, checkForSolidColor() may be called from frameAtIndex(). On iOS frameAtIndex() gets passed a scaleHint
    // argument which it uses to tell CG to create a scaled down image. Since we don't know the scaleHint here, if
    // we call frameAtIndex() again, we would pass it the default scale of 1 and would end up recreating the image.
    // So we do a quick check and call frameAtIndex(0) only if we haven't yet created an image.
    CGImageRef image = nullptr;
    if (m_frames.size())
        image = m_frames[0].m_frame;

    if (!image)
        image = frameAtIndex(0);
#endif

    // Currently we only check for solid color in the important special case of a 1x1 image.
    if (image && CGImageGetWidth(image) == 1 && CGImageGetHeight(image) == 1) {
        unsigned char pixel[4]; // RGBA
        RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreate(pixel, 1, 1, 8, sizeof(pixel), deviceRGBColorSpaceRef(),
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));
        if (!bitmapContext)
            return;
        GraphicsContext(bitmapContext.get()).setCompositeOperation(CompositeCopy);
        CGRect destinationRect = CGRectMake(0, 0, 1, 1);
        CGContextDrawImage(bitmapContext.get(), destinationRect, image);
        if (!pixel[3])
            m_solidColor = Color(0, 0, 0, 0);
        else
            m_solidColor = Color(pixel[0] * 255 / pixel[3], pixel[1] * 255 / pixel[3], pixel[2] * 255 / pixel[3], pixel[3]);

        m_isSolidColor = true;
    }
}

CGImageRef BitmapImage::getCGImageRef()
{
    return frameAtIndex(0);
}

CGImageRef BitmapImage::getFirstCGImageRefOfSize(const IntSize& size)
{
    size_t count = frameCount();
    for (size_t i = 0; i < count; ++i) {
        CGImageRef cgImage = frameAtIndex(i);
        if (cgImage && IntSize(CGImageGetWidth(cgImage), CGImageGetHeight(cgImage)) == size)
            return cgImage;
    }

    // Fallback to the default CGImageRef if we can't find the right size
    return getCGImageRef();
}

RetainPtr<CFArrayRef> BitmapImage::getCGImageArray()
{
    size_t count = frameCount();
    if (!count)
        return 0;
    
    CFMutableArrayRef array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < count; ++i) {
        if (CGImageRef currFrame = frameAtIndex(i))
            CFArrayAppendValue(array, currFrame);
    }
    return adoptCF(array);
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& destRect, const FloatRect& srcRect, ColorSpace styleColorSpace, CompositeOperator compositeOp, BlendMode blendMode, ImageOrientationDescription description)
{
    CGImageRef image;
#if !PLATFORM(IOS)
    startAnimation();

    image = frameAtIndex(m_currentFrame);
#else
    startAnimation(false);

    CGRect transformedDestinationRect = CGRectApplyAffineTransform(destRect, CGContextGetCTM(ctxt->platformContext()));
    RetainPtr<CGImageRef> imagePossiblyCopied;
    // Never use subsampled images for drawing into PDF contexts.
    if (CGContextGetType(ctxt->platformContext()) == kCGContextTypePDF)
        imagePossiblyCopied = adoptCF(copyUnscaledFrameAtIndex(m_currentFrame));
    else
        imagePossiblyCopied = frameAtIndex(m_currentFrame, std::min<float>(1.0f, std::max(transformedDestinationRect.size.width  / srcRect.width(), transformedDestinationRect.size.height / srcRect.height())));
    
    image = imagePossiblyCopied.get();
#endif
    if (!image) // If it's too early we won't have an image yet.
        return;
    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, destRect, solidColor(), styleColorSpace, compositeOp);
        return;
    }

    float scale = 1;
#if PLATFORM(IOS)
    scale = m_frames[m_currentFrame].m_scale;
#endif
    FloatSize selfSize = currentFrameSize();
    ImageOrientation orientation;

    if (description.respectImageOrientation() == RespectImageOrientation)
        orientation = frameOrientationAtIndex(m_currentFrame);

    ctxt->drawNativeImage(image, selfSize, styleColorSpace, destRect, srcRect, scale, compositeOp, blendMode, orientation);

    if (imageObserver())
        imageObserver()->didDraw(this);
}

#if PLATFORM(IOS)
PassNativeImagePtr BitmapImage::copyUnscaledFrameAtIndex(size_t index)
{
    if (index >= frameCount())
        return nullptr;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index, 1);

    if (m_frames[index].m_scale == 1 && !m_source.isSubsampled())
        return CGImageRetain(m_frames[index].m_frame);

    return m_source.createFrameAtIndex(index);
}
#endif

}

#endif // USE(CG)
