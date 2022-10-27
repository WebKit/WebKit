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

#include "DOMRectReadOnly.h"
#include "JSDOMPromiseDeferred.h"
#include "PlaneLayout.h"
#include "VideoColorSpaceInit.h"
#include "WebCodecsAlphaOption.h"
#include "WebCodecsVideoFrameData.h"

namespace WebCore {

class CSSStyleImageValue;
class DOMRectReadOnly;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class ImageBuffer;
class NativeImage;
class OffscreenCanvas;
class VideoColorSpace;

class WebCodecsVideoFrame : public RefCounted<WebCodecsVideoFrame> {
public:
    ~WebCodecsVideoFrame();

    using CanvasImageSource = std::variant<RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>, RefPtr<ImageBitmap>
        , RefPtr<CSSStyleImageValue>
#if ENABLE(OFFSCREEN_CANVAS)
        , RefPtr<OffscreenCanvas>
#endif
#if ENABLE(VIDEO)
        , RefPtr<HTMLVideoElement>
#endif
    >;

    enum class AlphaOption { Keep, Discard };
    struct Init {
        std::optional<uint64_t> duration;
        std::optional<int64_t> timestamp;
        WebCodecsAlphaOption alpha { WebCodecsAlphaOption::Keep };

        std::optional<DOMRectInit> visibleRect;

        std::optional<size_t> displayWidth;
        std::optional<size_t> displayHeight;
    };
    struct BufferInit {
        VideoPixelFormat format { VideoPixelFormat::I420 };
        size_t codedWidth { 0 };
        size_t codedHeight { 0 };
        int64_t timestamp { 0 };
        std::optional<uint64_t> duration;

        std::optional<Vector<PlaneLayout>> layout;

        std::optional<DOMRectInit> visibleRect;

        std::optional<size_t> displayWidth;
        std::optional<size_t> displayHeight;

        std::optional<VideoColorSpaceInit> colorSpace;
    };

    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(CanvasImageSource&&, Init&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(Ref<WebCodecsVideoFrame>&&, Init&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(BufferSource&&, BufferInit&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> create(ImageBuffer&, IntSize, Init&&);
    static Ref<WebCodecsVideoFrame> create(Ref<VideoFrame>&&, BufferInit&&);
    static Ref<WebCodecsVideoFrame> create(WebCodecsVideoFrameData&& data) { return adoptRef(*new WebCodecsVideoFrame(WTFMove(data))); }

    std::optional<VideoPixelFormat> format() const { return m_data.format; }
    size_t codedWidth() const { return m_data.codedWidth; }
    size_t codedHeight() const { return m_data.codedHeight; }

    DOMRectReadOnly* codedRect() const;
    DOMRectReadOnly* visibleRect() const;

    size_t displayWidth() const { return m_data.displayWidth; }
    size_t displayHeight() const { return m_data.displayHeight; }
    std::optional<uint64_t> duration() const { return m_data.duration; }
    int64_t timestamp() const { return m_data.timestamp; }
    VideoColorSpace* colorSpace() const;

    struct CopyToOptions {
        std::optional<DOMRectInit> rect;
        std::optional<Vector<PlaneLayout>> layout;
    };
    ExceptionOr<size_t> allocationSize(const CopyToOptions&);

    using CopyToPromise = DOMPromiseDeferred<IDLSequence<IDLDictionary<PlaneLayout>>>;
    void copyTo(BufferSource&&, CopyToOptions&&, CopyToPromise&&);
    ExceptionOr<Ref<WebCodecsVideoFrame>> clone();
    void close();

    bool isDetached() const { return m_isDetached; }
    RefPtr<VideoFrame> internalFrame() const { return m_data.internalFrame; }

    void setDisplaySize(size_t, size_t);
    void setVisibleRect(const DOMRectInit&);
    bool shoudlDiscardAlpha() const { return m_data.format && (*m_data.format == VideoPixelFormat::RGBX || *m_data.format == VideoPixelFormat::BGRX); }

    const WebCodecsVideoFrameData& data() const { return m_data; }

private:
    WebCodecsVideoFrame();
    explicit WebCodecsVideoFrame(WebCodecsVideoFrameData&&);

    static ExceptionOr<Ref<WebCodecsVideoFrame>> initializeFrameFromOtherFrame(Ref<WebCodecsVideoFrame>&&, Init&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> initializeFrameFromOtherFrame(Ref<VideoFrame>&&, Init&&);
    static ExceptionOr<Ref<WebCodecsVideoFrame>> initializeFrameWithResourceAndSize(Ref<NativeImage>&&, Init&&);

    WebCodecsVideoFrameData m_data;
    mutable RefPtr<VideoColorSpace> m_colorSpace;
    mutable RefPtr<DOMRectReadOnly> m_codedRect;
    mutable RefPtr<DOMRectReadOnly> m_visibleRect;
    bool m_isDetached { false };
};

}

#endif
