/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "DeferredImageDecoder.h"

#include "ImageDecodingStore.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

DeferredImageDecoder::DeferredImageDecoder(ImageDecoder* actualDecoder)
    : m_allDataReceived(false)
    , m_actualDecoder(adoptPtr(actualDecoder))
{
}

DeferredImageDecoder::~DeferredImageDecoder()
{
    // FIXME: Remove the corresponding entry in ImageDecodingStore if image
    // is defer-decoded.
}

DeferredImageDecoder* DeferredImageDecoder::create(const SharedBuffer& data, ImageSource::AlphaOption alphaOption, ImageSource::GammaAndColorProfileOption gammaAndColorOption)
{
    ImageDecoder* actualDecoder = ImageDecoder::create(data, alphaOption, gammaAndColorOption);
    return actualDecoder ? new DeferredImageDecoder(actualDecoder) : 0;
}

PassOwnPtr<DeferredImageDecoder> DeferredImageDecoder::createForTesting(PassOwnPtr<ImageDecoder> decoder)
{
    return adoptPtr(new DeferredImageDecoder(decoder.leakPtr()));
}

String DeferredImageDecoder::filenameExtension() const
{
    return m_actualDecoder ? m_actualDecoder->filenameExtension() : m_filenameExtension;
}

ImageFrame* DeferredImageDecoder::frameBufferAtIndex(size_t index)
{
    // Only defer image decoding if this is a single frame image. The reason is
    // because a multiframe is usually animated GIF. Animation is handled by
    // BitmapImage which uses some metadata functions that do synchronous image
    // decoding.
    if (ImageDecodingStore::instanceOnMainThread()
        && m_actualDecoder
        && m_actualDecoder->repetitionCount() == cAnimationNone
        && m_actualDecoder->isSizeAvailable()) {

        m_size = m_actualDecoder->size();
        m_filenameExtension = m_actualDecoder->filenameExtension();

        SkBitmap lazyDecodedSkBitmap = ImageDecodingStore::instanceOnMainThread()->createLazyDecodedSkBitmap(m_actualDecoder.release());
        m_lazyDecodedFrame.setSkBitmap(lazyDecodedSkBitmap);
        ImageDecodingStore::instanceOnMainThread()->setData(m_lazyDecodedFrame.getSkBitmap(), m_data.release(), m_allDataReceived);

        // Don't mark the frame as completely decoded until the underlying
        // decoder has really decoded it. Until then, our data and metadata may
        // be incorrect, so callers can't rely on them.
        m_lazyDecodedFrame.setStatus(ImageFrame::FramePartial);
    }

    return m_actualDecoder ? m_actualDecoder->frameBufferAtIndex(index) : &m_lazyDecodedFrame;
}

void DeferredImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_actualDecoder) {
        // Keep a reference to data until image decoding is deferred.
        // When image decoding is deferred then ownership of m_data is
        // transferred to ImageDecodingStore.
        m_data = data;
        m_allDataReceived = allDataReceived;
        m_actualDecoder->setData(data, allDataReceived);
    } else {
        ASSERT(!m_data);
        ImageDecodingStore::instanceOnMainThread()->setData(m_lazyDecodedFrame.getSkBitmap(), data, allDataReceived);
    }
}

bool DeferredImageDecoder::isSizeAvailable()
{
    // m_actualDecoder is 0 only if image decoding is deferred and that
    // means image header decoded successfully and size is available.
    return m_actualDecoder ? m_actualDecoder->isSizeAvailable() : true;
}

IntSize DeferredImageDecoder::size() const
{
    return m_actualDecoder ? m_actualDecoder->size() : m_size;
}

IntSize DeferredImageDecoder::frameSizeAtIndex(size_t index) const
{
    return m_actualDecoder ? m_actualDecoder->frameSizeAtIndex(index) : m_size;
}

size_t DeferredImageDecoder::frameCount()
{
    return m_actualDecoder ? m_actualDecoder->frameCount() : 1;
}

int DeferredImageDecoder::repetitionCount() const
{
    return m_actualDecoder ? m_actualDecoder->repetitionCount() : cAnimationNone;
}

void DeferredImageDecoder::clearFrameBufferCache(size_t clearBeforeFrame)
{
    // If image decoding is deferred then frame buffer cache is managed by
    // the compositor and this call is ignored.
    if (m_actualDecoder)
        m_actualDecoder->clearFrameBufferCache(clearBeforeFrame);
}

bool DeferredImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    // FIXME: Synchronize this state with ImageDecodingStore when image is
    // actually decoded. Return true here is correct in terms of rendering but
    // may not go through some optimized rendering code path.
    return m_actualDecoder ? m_actualDecoder->frameHasAlphaAtIndex(index) : true;
}

unsigned DeferredImageDecoder::frameBytesAtIndex(size_t index) const
{
    // If frame decoding is deferred then it is not managed by MemoryCache
    // so return 0 here.
    return m_actualDecoder ? m_actualDecoder->frameBytesAtIndex(index) : 0;
}

ImageOrientation DeferredImageDecoder::orientation() const
{
    // FIXME: Make this work with deferred decoding.
    return m_actualDecoder ? m_actualDecoder->orientation() : DefaultImageOrientation;
}

} // namespace WebCore
