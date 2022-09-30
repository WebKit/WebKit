/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_CODECS)

#include "DOMRectInit.h"
#include "JSDOMPromiseDeferred.h"
#include "PlaneLayout.h"
#include "VideoColorSpace.h"
#include "VideoFrame.h"
#include "VideoPixelFormat.h"

namespace WebCore {

class CSSStyleImageValue;
class DOMRectReadOnly;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class NativeImage;
class OffscreenCanvas;

using CanvasImageSource = std::variant<RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>, RefPtr<ImageBitmap>
#if ENABLE(CSS_TYPED_OM)
    , RefPtr<CSSStyleImageValue>
#endif
#if ENABLE(OFFSCREEN_CANVAS)
    , RefPtr<OffscreenCanvas>
#endif
#if ENABLE(VIDEO)
    , RefPtr<HTMLVideoElement>
#endif
    >;

class WebCodecsVideoFrame : public RefCounted<WebCodecsVideoFrame> {
public:
    ~WebCodecsVideoFrame();

    struct Init {
        std::optional<uint64_t> duration;
        std::optional<int64_t> timestamp;

        DOMRectInit visibleRect;

        std::optional<size_t> displayWidth;
        std::optional<size_t> displayHeight;
    };
    struct BufferInit {
        VideoPixelFormat format { VideoPixelFormat::I420 };
        size_t codedWidth { 0 };
        size_t codedHeight { 0 };
        int64_t timestamp { 0 };
        std::optional<uint64_t> duration { 0 };

        std::optional<Vector<PlaneLayout>> layout;

        std::optional<DOMRectInit> visibleRect;

        std::optional<size_t> displayWidth { 0 };
        std::optional<size_t> displayHeight { 0 };

        std::optional<VideoColorSpaceInit> colorSpace;
    };

    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(CanvasImageSource&&, Init&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(Ref<WebCodecsVideoFrame>&&, Init&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(BufferSource&&, BufferInit&&);

    std::optional<VideoPixelFormat> format() const { return m_format; }
    size_t codedWidth() const { return m_codedWidth; }
    size_t codedHeight() const { return m_codedHeight; }

    DOMRectReadOnly* codedRect() const;
    DOMRectReadOnly* visibleRect() const;

    size_t displayWidth() const { return m_displayWidth; }
    size_t displayHeight() const { return m_displayHeight; }
    std::optional<uint64_t> duration() const { return m_duration; }
    int64_t timestamp() const { return m_timestamp; }
    VideoColorSpace* colorSpace() const { return m_colorSpace.get(); }

    struct CopyToOptions {
        DOMRectInit rect;
        Vector<PlaneLayout> layout;
    };
    ExceptionOr<size_t> allocationSize(CopyToOptions&&);

    using CopyToPromise = DOMPromiseDeferred<IDLSequence<IDLDictionary<PlaneLayout>>>;
    void copyTo(BufferSource&&, CopyToOptions&&, CopyToPromise&&);
    ExceptionOr<Ref<WebCodecsVideoFrame>> clone();
    void close();

    bool isDetached() const { return m_isDetached; }

private:
    RefPtr<VideoFrame> m_internalFrame;
    std::optional<VideoPixelFormat> m_format;
    size_t m_codedWidth { 0 };
    size_t m_codedHeight { 0 };
    mutable RefPtr<DOMRectReadOnly> m_codedRect;
    size_t m_displayWidth { 0 };
    size_t m_displayHeight { 0 };
    size_t m_visibleWidth { 0 };
    size_t m_visibleHeight { 0 };
    size_t m_visibleLeft { 0 };
    size_t m_visibleTop { 0 };
    mutable RefPtr<DOMRectReadOnly> m_visibleRect;
    std::optional<uint64_t> m_duration { 0 };
    int64_t m_timestamp { 0 };
    RefPtr<VideoColorSpace> m_colorSpace;
    bool m_isDetached { false };
};

}

#endif
