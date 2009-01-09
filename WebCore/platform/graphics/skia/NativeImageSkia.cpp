/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "skia/ext/image_operations.h"

#include "NativeImageSkia.h"
#include "SkiaUtils.h"

NativeImageSkia::NativeImageSkia()
    : m_isDataComplete(false),
      m_lastRequestSize(0, 0),
      m_resizeRequests(0)
{
}

int NativeImageSkia::decodedSize() const
{
    return getSize() + m_resizedImage.getSize();
}

bool NativeImageSkia::hasResizedBitmap(int w, int h) const
{
    if (m_lastRequestSize.width() == w && m_lastRequestSize.height() == h)
        m_resizeRequests++;
    else {
        m_lastRequestSize = WebCore::IntSize(w, h);
        m_resizeRequests = 0;
    }

    return m_resizedImage.width() == w && m_resizedImage.height() == h;
}

// FIXME: don't cache when image is in-progress.

SkBitmap NativeImageSkia::resizedBitmap(int w, int h) const
{
    if (m_resizedImage.width() != w || m_resizedImage.height() != h)
        m_resizedImage = skia::ImageOperations::Resize(*this, skia::ImageOperations::RESIZE_LANCZOS3, w, h);

    return m_resizedImage;
}

bool NativeImageSkia::shouldCacheResampling(int destWidth,
                                            int destHeight,
                                            int destSubsetWidth,
                                            int destSubsetHeight) const
{
    // We can not cache incomplete frames. This might be a good optimization in
    // the future, were we know how much of the frame has been decoded, so when
    // we incrementally draw more of the image, we only have to resample the
    // parts that are changed.
    if (!m_isDataComplete)
        return false;

    // If the destination bitmap is small, we'll always allow caching, since
    // there is not very much penalty for computing it and it may come in handy.
    static const int kSmallBitmapSize = 4096;
    if (destWidth * destHeight <= kSmallBitmapSize)
        return true;

    // If "too many" requests have been made for this bitmap, we assume that
    // many more will be made as well, and we'll go ahead and cache it.
    static const int kManyRequestThreshold = 4;
    if (m_lastRequestSize.width() == destWidth &&
        m_lastRequestSize.height() == destHeight) {
        if (m_resizeRequests >= kManyRequestThreshold)
            return true;
    } else {
        // When a different size is being requested, count this as a query
        // (hasResizedBitmap) and reset the counter.
        m_lastRequestSize = WebCore::IntSize(destWidth, destHeight);
        m_resizeRequests = 0;
    }

    // Otherwise, use the heuristic that if more than 1/4 of the image is
    // requested, it's worth caching.
    int destSize = destWidth * destHeight;
    int destSubsetSize = destSubsetWidth * destSubsetHeight;
    return destSize / 4 < destSubsetSize;
}
