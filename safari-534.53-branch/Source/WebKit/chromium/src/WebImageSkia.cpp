/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebImage.h"

#include "Image.h"
#include "ImageSource.h"
#include "NativeImageSkia.h"
#include "SharedBuffer.h"

#include "WebData.h"
#include "WebSize.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

WebImage WebImage::fromData(const WebData& data, const WebSize& desiredSize)
{
    ImageSource source;
    source.setData(PassRefPtr<SharedBuffer>(data).get(), true);
    if (!source.isSizeAvailable())
        return WebImage();

    // Frames are arranged by decreasing size, then decreasing bit depth.
    // Pick the frame closest to |desiredSize|'s area without being smaller,
    // which has the highest bit depth.
    const size_t frameCount = source.frameCount();
    size_t index = 0;  // Default to first frame if none are large enough.
    int frameAreaAtIndex = 0;
    for (size_t i = 0; i < frameCount; ++i) {
        const IntSize frameSize = source.frameSizeAtIndex(i);
        if (WebSize(frameSize) == desiredSize) {
            index = i;
            break;  // Perfect match.
        }

        const int frameArea = frameSize.width() * frameSize.height();
        if (frameArea < (desiredSize.width * desiredSize.height))
            break;  // No more frames that are large enough.

        if (!i || (frameArea < frameAreaAtIndex)) {
            index = i;  // Closer to desired area than previous best match.
            frameAreaAtIndex = frameArea;
        }
    }

    OwnPtr<NativeImageSkia> frame = adoptPtr(source.createFrameAtIndex(index));
    if (!frame)
        return WebImage();

    return WebImage(*frame);
}

void WebImage::reset()
{
    m_bitmap.reset();
}

void WebImage::assign(const WebImage& image)
{
    m_bitmap = image.m_bitmap;
}

bool WebImage::isNull() const
{
    return m_bitmap.isNull();
}

WebSize WebImage::size() const
{
    return WebSize(m_bitmap.width(), m_bitmap.height());
}

WebImage::WebImage(const PassRefPtr<Image>& image)
{
    operator=(image);
}

WebImage& WebImage::operator=(const PassRefPtr<Image>& image)
{
    NativeImagePtr p;
    if (image.get() && (p = image->nativeImageForCurrentFrame()))
        assign(*p);
    else
        reset();
    return *this;
}

} // namespace WebKit
