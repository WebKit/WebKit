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

#include "config.h"
#include "WebCodecsVideoFrame.h"

#if ENABLE(WEB_CODECS)

#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "DOMRectReadOnly.h"
#include "ExceptionOr.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "JSPlaneLayout.h"
#include "OffscreenCanvas.h"
#include "PixelBuffer.h"
#include "VideoColorSpaceInit.h"
#include "WebCodecsVideoFrameAlgorithms.h"

#if PLATFORM(COCOA)
#include "VideoFrameCV.h"
#endif

namespace WebCore {

WebCodecsVideoFrame::~WebCodecsVideoFrame()
{
}

// https://html.spec.whatwg.org/multipage/canvas.html#check-the-usability-of-the-image-argument
static std::optional<Exception> checkImageUsability(const WebCodecsVideoFrame::CanvasImageSource& source)
{
    return switchOn(source,
    [] (const RefPtr<HTMLImageElement>& imageElement) -> std::optional<Exception> {
        auto* image = imageElement->cachedImage() ? imageElement->cachedImage()->image() : nullptr;
        if (!image)
            return Exception { InvalidStateError,  "Image element has no data"_s };
        if (!image->width() || !image->height())
            return Exception { InvalidStateError,  "Image element has a bad size"_s };
        return { };
    },
#if ENABLE(CSS_TYPED_OM)
    [] (const RefPtr<CSSStyleImageValue>& cssImage) -> std::optional<Exception> {
        auto* image = cssImage->image() ? cssImage->image()->image() : nullptr;
        if (!image)
            return Exception { InvalidStateError,  "CSS Image has no data"_s };
        if (!image->width() || !image->height())
            return Exception { InvalidStateError,  "CSS Image has a bad size"_s };
        return { };
    },
#endif
#if ENABLE(VIDEO)
    [] (const RefPtr<HTMLVideoElement>& video) -> std::optional<Exception> {
        auto readyState = video->readyState();
        if (readyState < HTMLMediaElement::HAVE_CURRENT_DATA)
            return Exception { InvalidStateError,  "Video element has no data"_s };
        return { };
    },
#endif
    [] (const RefPtr<HTMLCanvasElement>& canvas) -> std::optional<Exception> {
        auto size = canvas->size();
        if (!size.width() || !size.height())
            return Exception { InvalidStateError,  "Input canvas has a bad size"_s };
        return { };
    },
#if ENABLE(OFFSCREEN_CANVAS)
    [] (const RefPtr<OffscreenCanvas>& canvas) -> std::optional<Exception> {
        if (!canvas->width() || !canvas->height())
            return Exception { InvalidStateError,  "Input canvas has a bad size"_s };
        return { };
    },
#endif
    [] (const RefPtr<ImageBitmap>& image) -> std::optional<Exception> {
        if (image->isDetached())
            return Exception { InvalidStateError,  "Input ImageBitmap is detached"_s };
        return { };
    });
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(CanvasImageSource&& source, Init&& init)
{
    if (auto exception = checkImageUsability(source))
        return WTFMove(*exception);

    return switchOn(source,
    [&] (RefPtr<HTMLImageElement>& imageElement) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { TypeError,  "timestamp is not provided"_s };

        auto image = imageElement->cachedImage()->image()->nativeImageForCurrentFrame();
        if (!image)
            return Exception { InvalidStateError,  "Image element has no video frame"_s };

        return initializeFrameWithResourceAndSize(image.releaseNonNull(), WTFMove(init));
    },
#if ENABLE(CSS_TYPED_OM)
    [&] (RefPtr<CSSStyleImageValue>& cssImage) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { TypeError,  "timestamp is not provided"_s };

        auto image = cssImage->image()->image()->nativeImageForCurrentFrame();
        if (!image)
            return Exception { InvalidStateError,  "CSS Image has no video frame"_s };

        return initializeFrameWithResourceAndSize(image.releaseNonNull(), WTFMove(init));
    },
#endif
#if ENABLE(VIDEO)
    [&] (RefPtr<HTMLVideoElement>& video) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        RefPtr<VideoFrame> videoFrame = video->player() ? video->player()->videoFrameForCurrentTime() : nullptr;
        if (!videoFrame)
            return Exception { InvalidStateError,  "Video element has no video frame"_s };
        return initializeFrameFromOtherFrame(videoFrame.releaseNonNull(), WTFMove(init));
    },
#endif
    [&] (RefPtr<HTMLCanvasElement>& canvas) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { TypeError,  "timestamp is not provided"_s };

        if (!canvas->width() || !canvas->height())
            return Exception { InvalidStateError,  "Input canvas has a bad size"_s };

        auto videoFrame = canvas->toVideoFrame();
        if (!videoFrame)
            return Exception { InvalidStateError,  "Canvas has no frame"_s };
        return initializeFrameFromOtherFrame(videoFrame.releaseNonNull(), WTFMove(init));
    },
#if ENABLE(OFFSCREEN_CANVAS)
    [&] (RefPtr<OffscreenCanvas>& canvas) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { TypeError,  "timestamp is not provided"_s };

        if (!canvas->width() || !canvas->height())
            return Exception { InvalidStateError,  "Input canvas has a bad size"_s };

        auto* imageBuffer = canvas->buffer();
        if (!imageBuffer)
            return Exception { InvalidStateError,  "Input canvas has no image buffer"_s };

        return create(*imageBuffer, { static_cast<int>(canvas->width()), static_cast<int>(canvas->height()) }, WTFMove(init));
    },
#endif // ENABLE(OFFSCREEN_CANVAS)
    [&] (RefPtr<ImageBitmap>& image) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { TypeError,  "timestamp is not provided"_s };

        if (!image->width() || !image->height())
            return Exception { InvalidStateError,  "Input image has a bad size"_s };

        auto* imageBuffer = image->buffer();
        if (!imageBuffer)
            return Exception { InvalidStateError,  "Input image has no image buffer"_s };

        return create(*imageBuffer, { static_cast<int>(image->width()), static_cast<int>(image->height()) }, WTFMove(init));
    });
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(ImageBuffer& buffer, IntSize size, WebCodecsVideoFrame::Init&& init)
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    IntRect region { IntPoint::zero(), size };

    auto pixelBuffer = buffer.getPixelBuffer(format, region);
    if (!pixelBuffer)
        return Exception { InvalidStateError,  "Buffer has no frame"_s };

    RefPtr<VideoFrame> videoFrame;
#if PLATFORM(COCOA)
    videoFrame = VideoFrameCV::createFromPixelBuffer(pixelBuffer.releaseNonNull());
#endif
    if (!videoFrame)
        return Exception { InvalidStateError,  "Unable to create frame from buffer"_s };

    return WebCodecsVideoFrame::initializeFrameFromOtherFrame(videoFrame.releaseNonNull(), WTFMove(init));
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(Ref<WebCodecsVideoFrame>&& initFrame, Init&& init)
{
    if (initFrame->isDetached())
        return Exception { InvalidStateError,  "VideoFrame is detached"_s };
    return initializeFrameFromOtherFrame(WTFMove(initFrame), WTFMove(init));
}

// https://w3c.github.io/webcodecs/#dom-videoframe-videoframe-data-init
ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(BufferSource&& data, BufferInit&& init)
{
    if (!isValidVideoFrameBufferInit(init))
        return Exception { TypeError, "buffer init is not valid"_s };
    
    DOMRectInit defaultRect { 0, 0, static_cast<double>(init.codedWidth), static_cast<double>(init.codedHeight) };
    auto parsedRectOrExtension = parseVisibleRect(defaultRect, init.visibleRect, init.codedWidth, init.codedHeight, init.format);
    if (parsedRectOrExtension.hasException())
        return parsedRectOrExtension.releaseException();
    
    auto parsedRect = parsedRectOrExtension.releaseReturnValue();
    auto layoutOrException = computeLayoutAndAllocationSize(parsedRect, init.layout, init.format);
    if (layoutOrException.hasException())
        return layoutOrException.releaseException();
    
    auto layout = layoutOrException.releaseReturnValue();
    if (data.length() < layout.allocationSize)
        return Exception { TypeError, makeString("Data is too small ", data.length(), " / ", layout.allocationSize) };
    
    RefPtr<VideoFrame> videoFrame;
    if (init.format == VideoPixelFormat::NV12) {
        if (init.codedWidth % 2 || init.codedHeight % 2)
            return Exception { TypeError, "coded width or height is odd"_s };
        if (init.visibleRect && (static_cast<size_t>(init.visibleRect->x) % 2 || static_cast<size_t>(init.visibleRect->x) % 2))
            return Exception { TypeError, "visible x or y is odd"_s };
        videoFrame = VideoFrame::createNV12({ data.data(), data.length() }, parsedRect.width, parsedRect.height, layout.computedLayouts[0], layout.computedLayouts[1]);
    } else if (init.format == VideoPixelFormat::RGBA || init.format == VideoPixelFormat::RGBX)
        videoFrame = VideoFrame::createRGBA({ data.data(), data.length() }, parsedRect.width, parsedRect.height, layout.computedLayouts[0]);
    else if (init.format == VideoPixelFormat::BGRA || init.format == VideoPixelFormat::BGRX)
        videoFrame = VideoFrame::createBGRA({ data.data(), data.length() }, parsedRect.width, parsedRect.height, layout.computedLayouts[0]);
    else if (init.format == VideoPixelFormat::I420) {
        if (init.codedWidth % 2 || init.codedHeight % 2)
            return Exception { TypeError, "coded width or height is odd"_s };
        if (init.visibleRect && (static_cast<size_t>(init.visibleRect->x) % 2 || static_cast<size_t>(init.visibleRect->x) % 2))
            return Exception { TypeError, "visible x or y is odd"_s };
        videoFrame = VideoFrame::createI420({ data.data(), data.length() }, parsedRect.width, parsedRect.height, layout.computedLayouts[0], layout.computedLayouts[1], layout.computedLayouts[2]);
    } else
        return Exception { NotSupportedError, "VideoPixelFormat is not supported"_s };

    if (!videoFrame)
        return Exception { TypeError, "Unable to create internal resource from data"_s };
    
    return WebCodecsVideoFrame::create(videoFrame.releaseNonNull(), WTFMove(init));
}

Ref<WebCodecsVideoFrame> WebCodecsVideoFrame::create(Ref<VideoFrame>&& videoFrame, BufferInit&& init)
{
    ASSERT(isValidVideoFrameBufferInit(init));

    auto result = adoptRef(*new WebCodecsVideoFrame);
    result->m_internalFrame = WTFMove(videoFrame);
    result->m_format = init.format;

    result->m_codedWidth = result->m_internalFrame->presentationSize().width();
    result->m_codedHeight = result->m_internalFrame->presentationSize().height();

    result->m_visibleLeft = 0;
    result->m_visibleTop = 0;

    if (init.visibleRect) {
        result->m_visibleWidth = init.visibleRect->width;
        result->m_visibleHeight = init.visibleRect->height;
    } else {
        result->m_visibleWidth = result->m_codedWidth;
        result->m_visibleHeight = result->m_codedHeight;
    }

    result->m_displayWidth = init.displayWidth.value_or(result->m_visibleWidth);
    result->m_displayHeight = init.displayHeight.value_or(result->m_visibleHeight);

    result->m_duration = init.duration;
    result->m_timestamp = init.timestamp;
    result->m_colorSpace = videoFramePickColorSpace(init.colorSpace, *result->m_format);

    return result;
}

static VideoPixelFormat computeVideoPixelFormat(VideoPixelFormat baseFormat, bool shouldDiscardAlpha)
{
    if (!shouldDiscardAlpha)
        return baseFormat;
    switch (baseFormat) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I420A:
    case VideoPixelFormat::I422:
    case VideoPixelFormat::NV12:
    case VideoPixelFormat::I444:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRX:
        return baseFormat;
    case VideoPixelFormat::RGBA:
        return VideoPixelFormat::RGBX;
    case VideoPixelFormat::BGRA:
        return VideoPixelFormat::BGRX;
    }
    return baseFormat;
}

// https://w3c.github.io/webcodecs/#videoframe-initialize-frame-from-other-frame
ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::initializeFrameFromOtherFrame(Ref<WebCodecsVideoFrame>&& videoFrame, Init&& init)
{
    auto codedWidth = videoFrame->m_codedWidth;
    auto codedHeight = videoFrame->m_codedHeight;
    auto format = computeVideoPixelFormat(videoFrame->m_format.value_or(VideoPixelFormat::I420), init.alpha == WebCodecsAlphaOption::Discard);
    if (!validateVideoFrameInit(init, codedWidth, codedHeight, format))
        return Exception { TypeError,  "VideoFrameInit is not valid"_s };

    auto result = adoptRef(*new WebCodecsVideoFrame);
    result->m_internalFrame = videoFrame->m_internalFrame;
    if (videoFrame->m_format)
        result->m_format = format;

    result->m_codedWidth = videoFrame->m_codedWidth;
    result->m_codedHeight = videoFrame->m_codedHeight;
    result->m_colorSpace = videoFrame->m_colorSpace;

    initializeVisibleRectAndDisplaySize(result.get(), init, DOMRectInit { static_cast<double>(videoFrame->m_visibleLeft), static_cast<double>(videoFrame->m_visibleTop), static_cast<double>(videoFrame->m_visibleWidth), static_cast<double>(videoFrame->m_visibleHeight) }, videoFrame->m_displayWidth, videoFrame->m_displayHeight);

    result->m_duration = init.duration ? init.duration : videoFrame->m_duration;
    result->m_timestamp = init.timestamp.value_or(videoFrame->m_timestamp);

    return result;
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::initializeFrameFromOtherFrame(Ref<VideoFrame>&& internalVideoFrame, Init&& init)
{
    auto codedWidth = internalVideoFrame->presentationSize().width();
    auto codedHeight = internalVideoFrame->presentationSize().height();
    auto format = convertVideoFramePixelFormat(internalVideoFrame->pixelFormat(), init.alpha == WebCodecsAlphaOption::Discard);
    if (!validateVideoFrameInit(init, codedWidth, codedHeight, format))
        return Exception { TypeError,  "VideoFrameInit is not valid"_s };

    auto result = adoptRef(*new WebCodecsVideoFrame);
    result->m_internalFrame = WTFMove(internalVideoFrame);
    result->m_format = format;
    result->m_codedWidth = codedWidth;
    result->m_codedHeight = codedHeight;

    initializeVisibleRectAndDisplaySize(result.get(), init, DOMRectInit { 0, 0 , static_cast<double>(result->m_codedWidth), static_cast<double>(result->m_codedHeight) }, result->m_codedWidth, result->m_codedHeight);

    result->m_duration = init.duration;
    // FIXME: Use internalVideoFrame timestamp if available and init has no timestamp.
    result->m_timestamp = init.timestamp.value_or(0);

    return result;
}

// https://w3c.github.io/webcodecs/#videoframe-initialize-frame-with-resource-and-size
ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::initializeFrameWithResourceAndSize(Ref<NativeImage>&& image, Init&& init)
{
    auto internalVideoFrame = VideoFrame::fromNativeImage(image.get());
    if (!internalVideoFrame)
        return Exception { TypeError,  "image has no resource"_s };

    auto codedWidth = image->size().width();
    auto codedHeight = image->size().height();
    auto format = convertVideoFramePixelFormat(internalVideoFrame->pixelFormat(), init.alpha == WebCodecsAlphaOption::Discard);
    if (!validateVideoFrameInit(init, codedWidth, codedHeight, format))
        return Exception { TypeError,  "VideoFrameInit is not valid"_s };

    auto result = adoptRef(*new WebCodecsVideoFrame);
    result->m_internalFrame = WTFMove(internalVideoFrame);
    result->m_format = format;
    result->m_codedWidth = codedWidth;
    result->m_codedHeight = codedHeight;

    initializeVisibleRectAndDisplaySize(result.get(), init, DOMRectInit { 0, 0 , static_cast<double>(result->m_codedWidth), static_cast<double>(result->m_codedHeight) }, result->m_codedWidth, result->m_codedHeight);

    result->m_duration = init.duration;
    result->m_timestamp = init.timestamp.value_or(0);
    // FIXME: Set m_colorSpace

    return result;
}


ExceptionOr<size_t> WebCodecsVideoFrame::allocationSize(const CopyToOptions& options)
{
    if (isDetached())
        return Exception { InvalidStateError,  "VideoFrame is detached"_s };

    if (!m_format)
        return Exception { NotSupportedError,  "VideoFrame has no format"_s };

    auto layoutOrException = parseVideoFrameCopyToOptions(*this, options);
    if (layoutOrException.hasException())
        return layoutOrException.releaseException();

    return layoutOrException.returnValue().allocationSize;
}

void WebCodecsVideoFrame::copyTo(BufferSource&& source, CopyToOptions&& options, CopyToPromise&& promise)
{
    if (isDetached()) {
        promise.reject(Exception { InvalidStateError,  "VideoFrame is detached"_s });
        return;
    }
    if (!m_format) {
        promise.reject(Exception { NotSupportedError,  "VideoFrame has no format"_s });
        return;
    }

    auto combinedLayoutOrException = parseVideoFrameCopyToOptions(*this, options);
    if (combinedLayoutOrException.hasException()) {
        promise.reject(combinedLayoutOrException.releaseException());
        return;
    }

    auto combinedLayout = combinedLayoutOrException.releaseReturnValue();
    if (source.length() < combinedLayout.allocationSize) {
        promise.reject(Exception { TypeError,  "Buffer is too small"_s });
        return;
    }

    Span<uint8_t> buffer { static_cast<uint8_t*>(source.mutableData()), source.length() };
    m_internalFrame->copyTo(buffer, *m_format, WTFMove(combinedLayout.computedLayouts), [source = WTFMove(source), promise = WTFMove(promise)](auto planeLayouts) mutable {
        if (!planeLayouts) {
            promise.reject(Exception { TypeError,  "Unable to copy data"_s });
            return;
        }
        promise.resolve(WTFMove(*planeLayouts));
    });
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::clone()
{
    if (isDetached())
        return Exception { InvalidStateError,  "VideoFrame is detached"_s };

    auto clone = adoptRef(*new WebCodecsVideoFrame);
    clone->m_internalFrame = m_internalFrame;
    clone->m_isDetached = m_isDetached;
    clone->m_format = m_format;
    clone->m_codedWidth = m_codedWidth;
    clone->m_codedHeight = m_codedHeight;
    clone->m_codedRect = m_codedRect;
    clone->m_displayWidth = m_displayWidth;
    clone->m_displayHeight = m_displayHeight;
    clone->m_visibleWidth = m_visibleWidth;
    clone->m_visibleHeight = m_visibleHeight;
    clone->m_visibleLeft = m_visibleLeft;
    clone->m_visibleTop = m_visibleTop;
    clone->m_visibleRect = m_visibleRect;
    clone->m_duration = m_duration;
    clone->m_timestamp = m_timestamp;
    clone->m_colorSpace = m_colorSpace;
    return clone;
}

// https://w3c.github.io/webcodecs/#close-videoframe
void WebCodecsVideoFrame::close()
{
    m_internalFrame = nullptr;
    m_isDetached = true;
    m_format = { };
    m_codedWidth = 0;
    m_codedHeight = 0;
    m_codedRect = nullptr;
    m_displayWidth = 0;
    m_displayHeight = 0;
    m_visibleWidth = 0;
    m_visibleHeight = 0;
    m_visibleLeft = 0;
    m_visibleTop = 0;
    m_visibleRect = nullptr;
    m_duration = { };
    m_timestamp = 0;
    m_colorSpace = nullptr;
}

DOMRectReadOnly* WebCodecsVideoFrame::codedRect() const
{
    if (m_isDetached)
        return nullptr;
    if (!m_codedRect)
        m_codedRect = DOMRectReadOnly::create(0, 0, m_codedWidth, m_codedHeight);

    return m_codedRect.get();
}

DOMRectReadOnly* WebCodecsVideoFrame::visibleRect() const
{
    if (m_isDetached)
        return nullptr;
    if (!m_visibleRect)
        m_visibleRect = DOMRectReadOnly::create(m_visibleLeft, m_visibleTop, m_visibleWidth, m_visibleHeight);

    return m_visibleRect.get();
}

void WebCodecsVideoFrame::setDisplaySize(size_t width, size_t height)
{
    m_displayWidth = width;
    m_displayHeight = height;
}

void WebCodecsVideoFrame::setVisibleRect(const DOMRectInit& rect)
{
    m_visibleLeft = rect.x;
    m_visibleTop = rect.y;
    m_visibleWidth = rect.width;
    m_visibleHeight = rect.height;
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
