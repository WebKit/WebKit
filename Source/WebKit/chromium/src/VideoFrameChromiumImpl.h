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

#ifndef VideoFrameChromiumImpl_h
#define VideoFrameChromiumImpl_h

#include "VideoFrameChromium.h"
#include "WebVideoFrame.h"

using namespace WebCore;

namespace WebKit {

// A wrapper class for WebKit::WebVideoFrame. Objects can be created in WebKit
// and used in WebCore because of the VideoFrameChromium interface.
class VideoFrameChromiumImpl : public VideoFrameChromium {
public:
    // Converts a WebCore::VideoFrameChromium to a WebKit::WebVideoFrame.
    static WebVideoFrame* toWebVideoFrame(VideoFrameChromium*);

    // Creates a VideoFrameChromiumImpl object to wrap the given WebVideoFrame.
    // The VideoFrameChromiumImpl does not take ownership of the WebVideoFrame
    // and should not free the frame's memory.
    VideoFrameChromiumImpl(WebVideoFrame*);
    virtual SurfaceType surfaceType() const;
    virtual Format format() const;
    virtual unsigned width() const;
    virtual unsigned height() const;
    virtual unsigned planes() const;
    virtual int stride(unsigned plane) const;
    virtual const void* data(unsigned plane) const;
    virtual unsigned texture(unsigned plane) const;
    virtual const IntSize requiredTextureSize(unsigned plane) const;

private:
    WebVideoFrame* m_webVideoFrame;
};

} // namespace WebKit

#endif
