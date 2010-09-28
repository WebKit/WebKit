/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "VideoFrameChromiumImpl.h"

#include "VideoFrameChromium.h"
#include "WebVideoFrame.h"

using namespace WebCore;

const unsigned VideoFrameChromium::maxPlanes = 3;
const unsigned VideoFrameChromium::numRGBPlanes = 1;
const unsigned VideoFrameChromium::rgbPlane = 0;
const unsigned VideoFrameChromium::numYUVPlanes = 3;
const unsigned VideoFrameChromium::yPlane = 0;
const unsigned VideoFrameChromium::uPlane = 1;
const unsigned VideoFrameChromium::vPlane = 2;

namespace WebKit {

WebVideoFrame* VideoFrameChromiumImpl::toWebVideoFrame(VideoFrameChromium* videoFrame)
{
    VideoFrameChromiumImpl* wrappedFrame = static_cast<VideoFrameChromiumImpl*>(videoFrame);
    if (wrappedFrame)
        return wrappedFrame->m_webVideoFrame;
    return 0;
}

VideoFrameChromiumImpl::VideoFrameChromiumImpl(WebVideoFrame* webVideoFrame)
    : m_webVideoFrame(webVideoFrame)
{
}

VideoFrameChromium::SurfaceType VideoFrameChromiumImpl::surfaceType() const
{
    if (m_webVideoFrame)
        return static_cast<VideoFrameChromium::SurfaceType>(m_webVideoFrame->surfaceType());
    return TypeSystemMemory;
}

VideoFrameChromium::Format VideoFrameChromiumImpl::format() const
{
    if (m_webVideoFrame)
        return static_cast<VideoFrameChromium::Format>(m_webVideoFrame->format());
    return Invalid;
}

unsigned VideoFrameChromiumImpl::width() const
{
    if (m_webVideoFrame)
        return m_webVideoFrame->width();
    return 0;
}

unsigned VideoFrameChromiumImpl::height() const
{
    if (m_webVideoFrame)
        return m_webVideoFrame->height();
    return 0;
}

unsigned VideoFrameChromiumImpl::planes() const
{
    if (m_webVideoFrame)
        return m_webVideoFrame->planes();
    return 0;
}

int VideoFrameChromiumImpl::stride(unsigned plane) const
{
    if (m_webVideoFrame)
        return m_webVideoFrame->stride(plane);
    return 0;
}

const void* VideoFrameChromiumImpl::data(unsigned plane) const
{
    if (m_webVideoFrame)
        return m_webVideoFrame->data(plane);
    return 0;
}

const IntSize VideoFrameChromiumImpl::requiredTextureSize(unsigned plane) const
{
    switch (format()) {
    case RGBA:
        return IntSize(stride(plane), height());
    case YV12:
        switch (plane) {
        case yPlane:
            return IntSize(stride(plane), height());
        case uPlane:
            return IntSize(stride(plane), height() / 2);
        case vPlane:
            return IntSize(stride(plane), height() / 2);
        default:
            break;
        }
    }
    return IntSize();
}

} // namespace WebKit
