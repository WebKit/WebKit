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
#include "SharedBuffer.h"

#include "WebData.h"
#include "WebSize.h"

#include <CoreGraphics/CGImage.h>

#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

WebImage WebImage::fromData(const WebData& data, const WebSize& desiredSize)
{
    // FIXME: Do something like what WebImageSkia.cpp does to enumerate frames.
    // Not sure whether the CG decoder uses the same frame ordering rules (if so
    // we can just use the same logic).

    ImageSource source;
    source.setData(PassRefPtr<SharedBuffer>(data).get(), true);
    if (!source.isSizeAvailable())
        return WebImage();

    RetainPtr<CGImageRef> frame0(AdoptCF, source.createFrameAtIndex(0));
    if (!frame0)
        return WebImage();

    return WebImage(frame0.get());
}

void WebImage::reset()
{
    CGImageRelease(m_imageRef);
    m_imageRef = 0;
}

void WebImage::assign(const WebImage& image)
{
    assign(image.m_imageRef);
}

bool WebImage::isNull() const
{
    return !m_imageRef;
}

WebSize WebImage::size() const
{
    return WebSize(CGImageGetWidth(m_imageRef), CGImageGetHeight(m_imageRef));
}

WebImage::WebImage(const PassRefPtr<Image>& image)
    : m_imageRef(0)
{
    if (image.get())
        assign(image->nativeImageForCurrentFrame());
}

WebImage& WebImage::operator=(const PassRefPtr<Image>& image)
{
    if (image.get())
        assign(image->nativeImageForCurrentFrame());
    else
        reset();
    return *this;
}

void WebImage::assign(CGImageRef imageRef)
{
    // Make sure to retain the imageRef first incase m_imageRef == imageRef.
    CGImageRetain(imageRef);
    CGImageRelease(m_imageRef);
    m_imageRef = imageRef;
}

} // namespace WebKit
