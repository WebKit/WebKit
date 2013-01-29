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
#include "ImageFrameGenerator.h"
#include "LazyDecodingPixelRef.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

namespace {

// URI label for a lazily decoded SkPixelRef.
const char labelLazyDecoded[] = "lazy";

} // namespace

bool DeferredImageDecoder::s_enabled = false;

DeferredImageDecoder::DeferredImageDecoder(ImageDecoder* actualDecoder)
    : m_allDataReceived(false)
    , m_actualDecoder(adoptPtr(actualDecoder))
    , m_orientation(DefaultImageOrientation)
{
}

DeferredImageDecoder::~DeferredImageDecoder()
{
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

bool DeferredImageDecoder::isLazyDecoded(const SkBitmap& bitmap)
{
    return bitmap.pixelRef()
        && bitmap.pixelRef()->getURI()
        && !memcmp(bitmap.pixelRef()->getURI(), labelLazyDecoded, sizeof(labelLazyDecoded));
}

SkBitmap DeferredImageDecoder::createResizedLazyDecodingBitmap(const SkBitmap& bitmap, const SkISize& scaledSize, const SkIRect& scaledSubset)
{
    LazyDecodingPixelRef* pixelRef = static_cast<LazyDecodingPixelRef*>(bitmap.pixelRef());

    int rowBytes = 0;
    rowBytes = SkBitmap::ComputeRowBytes(SkBitmap::kARGB_8888_Config, scaledSize.width());

    SkBitmap resizedBitmap;
    resizedBitmap.setConfig(SkBitmap::kARGB_8888_Config, scaledSubset.width(), scaledSubset.height(), rowBytes);

    // FIXME: This code has the potential problem that multiple
    // LazyDecodingPixelRefs are created even though they share the same
    // scaled size and ImageFrameGenerator.
    resizedBitmap.setPixelRef(new LazyDecodingPixelRef(pixelRef->frameGenerator(), scaledSize, scaledSubset))->unref();

    // See comments in createLazyDecodingBitmap().
    resizedBitmap.setImmutable();
    return resizedBitmap;
}

void DeferredImageDecoder::setEnabled(bool enabled)
{
    s_enabled = enabled;
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
    if (s_enabled
        && m_actualDecoder
        && m_actualDecoder->repetitionCount() == cAnimationNone
        && m_actualDecoder->isSizeAvailable()
        && m_actualDecoder->frameCount() == 1) {

        m_size = m_actualDecoder->size();
        m_filenameExtension = m_actualDecoder->filenameExtension();
        m_orientation = m_actualDecoder->orientation();

        SkBitmap lazyDecodedSkBitmap = createLazyDecodingBitmap();
        m_lazyDecodedFrame.setSkBitmap(lazyDecodedSkBitmap);

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
        m_frameGenerator->setData(data, allDataReceived);
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
    return m_actualDecoder ? m_actualDecoder->orientation() : m_orientation;
}

SkBitmap DeferredImageDecoder::createLazyDecodingBitmap()
{
    SkISize fullSize = SkISize::Make(m_actualDecoder->size().width(), m_actualDecoder->size().height());
    ASSERT(!fullSize.isEmpty());

    SkIRect fullRect = SkIRect::MakeSize(fullSize);

    // Creates a lazily decoded SkPixelRef that references the entire image without scaling.
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, fullSize.width(), fullSize.height());

    m_frameGenerator = ImageFrameGenerator::create(fullSize, m_data.release(), m_allDataReceived);
    m_actualDecoder.clear();

    bitmap.setPixelRef(new LazyDecodingPixelRef(m_frameGenerator, fullSize, fullRect))->unref();

    // Use the URI to identify this as a lazily decoded SkPixelRef of type LazyDecodingPixelRef.
    // FIXME: It would be more useful to give the actual image URI.
    bitmap.pixelRef()->setURI(labelLazyDecoded);

    // Inform the bitmap that we will never change the pixels. This is a performance hint
    // subsystems that may try to cache this bitmap (e.g. pictures, pipes, gpu, pdf, etc.)
    bitmap.setImmutable();

    // FIXME: Setting bitmap.setIsOpaque() is big performance gain if possible. We can
    // do so safely if the image is fully loaded and it is a JPEG image, or if the image was
    // decoded before.

    return bitmap;
}

bool DeferredImageDecoder::hotSpot(IntPoint& hotSpot) const
{
    return m_actualDecoder ? m_actualDecoder->hotSpot(hotSpot) : false;
}

} // namespace WebCore
