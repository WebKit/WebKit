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

#include "ImageFrameGenerator.h"

#include "ImageDecoder.h"
#include "SharedBuffer.h"

#include "skia/ext/image_operations.h"

namespace WebCore {

ImageFrameGenerator::ImageFrameGenerator(PassRefPtr<SharedBuffer> data, bool allDataReceived)
    : m_allDataReceived(false)
{
    setData(data, allDataReceived);
}

ImageFrameGenerator::~ImageFrameGenerator()
{
}

void ImageFrameGenerator::setData(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    // FIXME: Doing a full copy is expensive, instead copy only new data.
    RefPtr<SharedBuffer> dataCopy = data->copy();

    MutexLocker lock(m_mutex);
    m_data = dataCopy;
    m_allDataReceived = allDataReceived;
}

SkBitmap ImageFrameGenerator::decodeAndScale(const SkISize& scaledSize, const SkIRect& scaledSubset)
{
    RefPtr<SharedBuffer> data;
    bool allDataReceived = false;
    {
        MutexLocker lock(m_mutex);

        // FIXME: We should do a shallow copy instead. Now we're restricted by the API of SharedBuffer.
        data = m_data->copy();
        allDataReceived = m_allDataReceived;
    }

    OwnPtr<ImageDecoder> decoder(adoptPtr(ImageDecoder::create(*data.get(), ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileApplied)));
    if (!decoder)
        return SkBitmap();

    decoder->setData(data.get(), allDataReceived);
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    if (!frame)
        return SkBitmap();

    SkBitmap bitmap = frame->getSkBitmap();
    SkISize bitmapSize = SkISize::Make(bitmap.width(), bitmap.height());
    if (bitmapSize != scaledSize)
        bitmap = skia::ImageOperations::Resize(bitmap, skia::ImageOperations::RESIZE_LANCZOS3, scaledSize.width(), scaledSize.height());

    SkBitmap bitmapSubset;
    bitmap.extractSubset(&bitmapSubset, scaledSubset);
    return bitmapSubset;
}

} // namespace WebCore
