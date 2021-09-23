/*
 * Copyright (C) 2020 Microsoft Corporation.
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

#pragma once

#include <WebCore/IntSize.h>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

#if USE(CAIRO)
#include <cairo.h>
#endif

namespace WebKit {

class WebPageProxy;

class ScreencastEncoder : public ThreadSafeRefCounted<ScreencastEncoder> {
    WTF_MAKE_NONCOPYABLE(ScreencastEncoder);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr int fps = 25;

    static RefPtr<ScreencastEncoder> create(String& errorString, const String& filePath, WebCore::IntSize, std::optional<double> scale);

    class VPXCodec;
    ScreencastEncoder(std::unique_ptr<VPXCodec>&&, WebCore::IntSize, std::optional<double> scale);
    ~ScreencastEncoder();

#if USE(CAIRO)
    void encodeFrame(cairo_surface_t*, WebCore::IntSize);
#elif PLATFORM(MAC)
    void encodeFrame(RetainPtr<CGImageRef>&&);
    void setOffsetTop(int offset) { m_offsetTop = offset;}
#endif

    void finish(Function<void()>&& callback);

private:
    void flushLastFrame();
#if PLATFORM(MAC)
    static void imageToARGB(CGImageRef, uint8_t* rgba_data, int width, int height, std::optional<double> scale, int offsetTop);
#endif

    std::unique_ptr<VPXCodec> m_vpxCodec;
    const WebCore::IntSize m_size;
    std::optional<double> m_scale;
    MonotonicTime m_lastFrameTimestamp;
    class VPXFrame;
    std::unique_ptr<VPXFrame> m_lastFrame;
#if PLATFORM(MAC)
    int m_offsetTop { 0 };
#endif
};

} // namespace WebKit
