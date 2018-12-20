/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "DisplayCaptureSourceCocoa.h"

typedef struct __CVBuffer *CVBufferRef;
typedef CVBufferRef CVImageBufferRef;
typedef CVImageBufferRef CVPixelBufferRef;
typedef struct __CVPixelBufferPool *CVPixelBufferPoolRef;

namespace WebCore {

class PixelBufferConformerCV;

class WindowDisplayCaptureSourceMac : public DisplayCaptureSourceCocoa {
public:
    static CaptureSourceOrError create(String&&, const MediaConstraints*);

    static Optional<CaptureDevice> windowCaptureDeviceWithPersistentID(const String&);
    static void windowCaptureDevices(Vector<CaptureDevice>&);

private:
    WindowDisplayCaptureSourceMac(uint32_t, String&&);
    virtual ~WindowDisplayCaptureSourceMac() = default;

    DisplayCaptureSourceCocoa::DisplayFrameType generateFrame() final;
    RealtimeMediaSourceSettings::DisplaySurfaceType surfaceType() const final { return RealtimeMediaSourceSettings::DisplaySurfaceType::Window; }

    RetainPtr<CGImageRef> windowImage();

    IntSize m_windowSize;
    CGWindowID m_windowID { 0 };
    RetainPtr<CVPixelBufferPoolRef> m_bufferPool;
    std::unique_ptr<PixelBufferConformerCV> m_pixelBufferConformer;
    RetainPtr<CGImageRef> m_lastImage;
    RetainPtr<CVPixelBufferRef> m_lastPixelBuffer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
