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

#ifndef ImageResizerThread_h
#define ImageResizerThread_h

#if ENABLE(IMAGE_RESIZER)

#include "AsyncImageResizer.h"

namespace WebCore {

class BitmapImage;
class SharedBuffer;

class ImageResizerThread {
public:
    static bool start(PassRefPtr<SharedBuffer> imageData, AsyncImageResizer::CallbackInfo*, AsyncImageResizer::OutputType, IntSize desiredBounds, float quality, AsyncImageResizer::AspectRatioOption, AsyncImageResizer::OrientationOption);
    ~ImageResizerThread();

private:
    ImageResizerThread(AsyncImageResizer::CallbackInfo*, AsyncImageResizer::OutputType, IntSize desiredBounds, float quality, AsyncImageResizer::AspectRatioOption, AsyncImageResizer::OrientationOption);
    static void* imageResizerThreadStart(void*);
    void* imageResizerThread();

    // Threading attributes.
    ThreadIdentifier m_threadID;

    // Image attributes.
    AsyncImageResizer::CallbackInfo* m_callbackInfo;
    RefPtr<BitmapImage> m_image;
    AsyncImageResizer::OutputType m_outputType;
    IntSize m_desiredBounds;
    float m_quality;
    AsyncImageResizer::AspectRatioOption m_aspectRatioOption;
    AsyncImageResizer::OrientationOption m_orientationOption;
};

} // namespace WebCore

#endif // ENABLE(IMAGE_RESIZER)

#endif // ImageResizerThread_h
