/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include <kxmlcore/Vector.h>
#include "Array.h"
#include "IntSize.h"
#include "FloatRect.h"
#include "ImageDecoder.h"
#include "ImageAnimationObserver.h"
#include "ImageData.h"
#include "Image.h"
#include "Timer.h"

#if __APPLE__
#include "PDFDocumentImage.h"
#endif

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
#if __APPLE__
        CFRelease(m_frame);
#else
        delete m_frame;
#endif
        m_frame = 0;
        m_duration = 0.;
    }
}

ImageData::ImageData(Image* image) 
     : m_image(image), m_currentFrame(0), m_frames(0),
#if __APPLE__
       m_nsImage(0), m_tiffRep(0),
#endif
       m_frameTimer(0), m_repetitionCount(0), m_repetitionsComplete(0),
#if __APPLE__
       m_solidColor(0), m_isSolidColor(0),
#endif
       m_animatingImageType(true), m_animationFinished(0),
       m_haveSize(false), m_sizeAvailable(false),
#if __APPLE__
       m_isPDF(false), m_PDFDoc(0)
#endif
{}

ImageData::~ImageData()
{
    invalidateData();
    stopAnimation();
#if __APPLE__
    delete m_PDFDoc;
#endif
}

void ImageData::invalidateData()
{
    // Destroy the cached images and release them.
    if (m_frames.size()) {
        m_frames.last().clear();
#if __APPLE__
        invalidateAppleSpecificData();
#endif
    }
}

void ImageData::cacheFrame(size_t index)
{
    size_t numFrames = frameCount();
    if (!m_frames.size() && shouldAnimate()) {            
        // Snag the repetition count.
        m_repetitionCount = m_decoder.repetitionCount();
        if (m_repetitionCount == cAnimationNone)
            m_animatingImageType = false;
    }
    
    if (m_frames.size() < numFrames)
        m_frames.resize(numFrames);

    m_frames[index].m_frame = m_decoder.createFrameAtIndex(index);
    if (shouldAnimate())
        m_frames[index].m_duration = m_decoder.frameDurationAtIndex(index);
}

bool ImageData::isNull() const
{
    return size().isEmpty();
}

IntSize ImageData::size() const
{
#if __APPLE__
    if (m_isPDF) {
        if (m_PDFDoc) {
            CGRect mediaBox = m_PDFDoc->mediaBox();
            return IntSize((int)mediaBox.size.width, (int)mediaBox.size.height);
        }
    } 
    else
#endif
    if (m_sizeAvailable && !m_haveSize && frameCount() > 0) {
        m_size = m_decoder.size();
        m_haveSize = true;
    }
    return m_size;
}

bool ImageData::setData(const ByteArray& bytes, bool allDataReceived)
{
    int length = bytes.count();
    if (length == 0)
        return true;

#ifdef kImageBytesCutoff
    // This is a hack to help with testing display of partially-loaded images.
    // To enable it, define kImageBytesCutoff to be a size smaller than that of the image files
    // being loaded. They'll never finish loading.
    if (length > kImageBytesCutoff) {
        length = kImageBytesCutoff;
        allDataReceived = false;
    }
#endif
    
#if __APPLE__
    // Avoid the extra copy of bytes by just handing the byte array directly to a CFDataRef.
    // We will keep these bytes alive in our m_data variable.
    CFDataRef data = CFDataCreateWithBytesNoCopy(0, (const UInt8*)bytes.data(), length, kCFAllocatorNull);
    bool result = setNativeData(data, allDataReceived);
    CFRelease(data);
#else
    bool result = setNativeData(&bytes, allDataReceived);
#endif

    m_data = bytes;
    return result;
}

bool ImageData::setNativeData(NativeBytePtr data, bool allDataReceived)
{
#if __APPLE__
    if (m_isPDF) {
        if (allDataReceived && !m_PDFDoc)
            m_PDFDoc = new PDFDocumentImage((NSData*)data);
        return true;
    }
#endif

    invalidateData();
    
    // Feed all the data we've seen so far to the image decoder.
    m_decoder.setData(data, allDataReceived);
    
    // Image properties will not be available until the first frame of the file
    // reaches kCGImageStatusIncomplete.
    return isSizeAvailable();
}

size_t ImageData::frameCount() const
{
    return m_decoder.frameCount();
}

bool ImageData::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_decoder.isSizeAvailable();

    return m_sizeAvailable;

}

CGImageRef ImageData::frameAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_frame;
}

float ImageData::frameDurationAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_duration;
}

bool ImageData::shouldAnimate() const
{
    return (m_animatingImageType && frameCount() > 1 && !m_animationFinished && m_image->animationObserver());
}

void ImageData::startAnimation()
{
    if (m_frameTimer || !shouldAnimate())
        return;

    m_frameTimer = new Timer<ImageData>(this, &ImageData::advanceAnimation);
    m_frameTimer->startOneShot(frameDurationAtIndex(m_currentFrame));
}

void ImageData::stopAnimation()
{
    // This timer is used to animate all occurrences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
    delete m_frameTimer;
    m_frameTimer = 0;
}

void ImageData::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_animationFinished = false;
}

void ImageData::advanceAnimation(Timer<ImageData>* timer)
{
    // Stop the animation.
    stopAnimation();
    
    // See if anyone is still paying attention to this animation.  If not, we don't
    // advance and will simply pause the animation.
    if (m_image->animationObserver()->shouldStopAnimation(m_image))
        return;

    m_currentFrame++;
    if (m_currentFrame >= frameCount()) {
        m_repetitionsComplete += 1;
        if (m_repetitionCount && m_repetitionsComplete >= m_repetitionCount) {
            m_animationFinished = false;
            m_currentFrame--;
            return;
        }
        m_currentFrame = 0;
    }

    // Notify our observer that the animation has advanced.
    m_image->animationObserver()->animationAdvanced(m_image);
        
    // Kick off a timer to move to the next frame.
    m_frameTimer = new Timer<ImageData>(this, &ImageData::advanceAnimation);
    m_frameTimer->startOneShot(frameDurationAtIndex(m_currentFrame));
}

}
