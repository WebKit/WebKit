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
#include "BitmapImage.h"

#if USE(CG)

#include "CoreGraphicsSPI.h"
#include "FloatConversion.h"
#include "GeometryUtilities.h"
#include "GraphicsContextCG.h"
#include "ImageObserver.h"
#include "SubimageCacheWithTimer.h"
#include <wtf/RetainPtr.h>

#if USE(APPKIT)
#include <ApplicationServices/ApplicationServices.h>
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
    m_subsamplingLevel = 0;

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
    , m_minimumSubsamplingLevel(0)
    , m_imageOrientation(OriginTopLeft)
    , m_shouldRespectImageOrientation(false)
    , m_currentFrame(0)
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
    m_sizeRespectingOrientation = m_size;

    m_frames.grow(1);
    m_frames[0].m_frame = CGImageRetain(cgImage);
    m_frames[0].m_hasAlpha = true;
    m_frames[0].m_haveMetadata = true;

    checkForSolidColor();
}

void BitmapImage::determineMinimumSubsamplingLevel() const
{
    if (!m_allowSubsampling)
        return;

    if (!m_source.allowSubsamplingOfFrameAtIndex(0))
        return;

    // Values chosen to be appropriate for iOS.
    const int cMaximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    const SubsamplingLevel maxSubsamplingLevel = 3;

    SubsamplingLevel currentLevel = 0;
    for ( ; currentLevel <= maxSubsamplingLevel; ++currentLevel) {
        IntSize frameSize = m_source.frameSizeAtIndex(0, currentLevel);
        if (frameSize.area() < cMaximumImageAreaBeforeSubsampling)
            break;
    }

    m_minimumSubsamplingLevel = currentLevel;
}

void BitmapImage::checkForSolidColor()
{
    m_checkedForSolidColor = true;
    m_isSolidColor = false;

    if (frameCount() > 1)
        return;

    if (!haveFrameAtIndex(0)) {
        IntSize size = m_source.frameSizeAtIndex(0, 0);
        if (size.width() != 1 || size.height() != 1)
            return;

        if (!ensureFrameIsCached(0))
            return;
    }

    CGImageRef image = nullptr;
    if (m_frames.size())
        image = m_frames[0].m_frame;

    if (!image)
        return;

    // Currently we only check for solid color in the important special case of a 1x1 image.
    if (CGImageGetWidth(image) == 1 && CGImageGetHeight(image) == 1) {
        unsigned char pixel[4]; // RGBA
        RetainPtr<CGContextRef> bitmapContext = adoptCF(CGBitmapContextCreate(pixel, 1, 1, 8, sizeof(pixel), sRGBColorSpaceRef(),
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

void BitmapImage::draw(GraphicsContext& ctxt, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOp, BlendMode blendMode, ImageOrientationDescription description)
{
#if PLATFORM(IOS)
    startAnimation(DoNotCatchUp);
#else
    startAnimation();
#endif

    RetainPtr<CGImageRef> image;
    // Never use subsampled images for drawing into PDF contexts.
    if (wkCGContextIsPDFContext(ctxt.platformContext()))
        image = adoptCF(copyUnscaledFrameAtIndex(m_currentFrame));
    else {
        CGRect transformedDestinationRect = CGRectApplyAffineTransform(destRect, CGContextGetCTM(ctxt.platformContext()));
        float subsamplingScale = std::min<float>(1, std::max(transformedDestinationRect.size.width / srcRect.width(), transformedDestinationRect.size.height / srcRect.height()));

        image = frameAtIndex(m_currentFrame, subsamplingScale);
    }

    if (!image) // If it's too early we won't have an image yet.
        return;
    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, destRect, solidColor(), compositeOp);
        return;
    }

    // Subsampling may have given us an image that is smaller than size().
    IntSize imageSize(CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
    
    // srcRect is in the coordinates of the unsubsampled image, so we have to map it to the subsampled image.
    FloatRect scaledSrcRect = srcRect;
    if (imageSize != m_size) {
        FloatRect originalImageBounds(FloatPoint(), m_size);
        FloatRect subsampledImageBounds(FloatPoint(), imageSize);
        scaledSrcRect = mapRect(srcRect, originalImageBounds, subsampledImageBounds);
    }
    
    ImageOrientation orientation;
    if (description.respectImageOrientation() == RespectImageOrientation)
        orientation = frameOrientationAtIndex(m_currentFrame);

    ctxt.drawNativeImage(image.get(), imageSize, destRect, scaledSrcRect, compositeOp, blendMode, orientation);

    if (imageObserver())
        imageObserver()->didDraw(this);
}

PassNativeImagePtr BitmapImage::copyUnscaledFrameAtIndex(size_t index)
{
    if (index >= frameCount())
        return nullptr;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index, 0);

    if (!m_frames[index].m_subsamplingLevel)
        return CGImageRetain(m_frames[index].m_frame);

    return m_source.createFrameAtIndex(index);
}

}

#endif // USE(CG)
