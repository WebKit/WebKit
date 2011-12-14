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

#ifndef VideoFrameProvider_h
#define VideoFrameProvider_h

#include "VideoFrameChromium.h"

namespace WebCore {

class VideoFrameProvider {
public:
    virtual ~VideoFrameProvider() { }

    // This function returns a pointer to a VideoFrameChromium, which is
    // the WebCore wrapper for a video frame in Chromium. getCurrentFrame()
    // places a lock on the frame in Chromium. Calls to this method should
    // always be followed with a call to putCurrentFrame().
    // The ownership of the object is not transferred to the caller and
    // the caller should not free the returned object.
    virtual VideoFrameChromium* getCurrentFrame() = 0;
    // This function releases the lock on the video frame in chromium. It should
    // always be called after getCurrentFrame(). Frames passed into this method
    // should no longer be referenced after the call is made.
    virtual void putCurrentFrame(VideoFrameChromium*) = 0;
};

} // namespace WebCore

#endif
