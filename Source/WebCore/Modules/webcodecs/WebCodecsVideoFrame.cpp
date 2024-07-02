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
#include "JSDOMPromiseDeferred.h"
#include "JSPlaneLayout.h"
#include "OffscreenCanvas.h"
#include "PixelBuffer.h"
#include "SVGImageElement.h"
#include "SecurityOrigin.h"
#include "VideoColorSpace.h"
#include "WebCodecsVideoFrameAlgorithms.h"
#include <wtf/Seconds.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include "VideoFrameCV.h"
#endif

#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif

namespace WebCore {

static MediaTime timestampToMediaTime(int64_t timestamp)
{
    return MediaTime::createWithDouble(Seconds::fromMicroseconds(timestamp).value());
}

static int64_t mediaTimeToTimestamp(MediaTime mediaTime)
{
    return Seconds(mediaTime.toDouble()).microseconds();
}

WebCodecsVideoFrame::WebCodecsVideoFrame(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
{
}

WebCodecsVideoFrame::WebCodecsVideoFrame(ScriptExecutionContext& context, WebCodecsVideoFrameData&& data)
    : ContextDestructionObserver(&context)
    , m_data(WTFMove(data))
{
}

WebCodecsVideoFrame::~WebCodecsVideoFrame()
{
    if (m_isDetached)
        return;
    if (RefPtr context = scriptExecutionContext()) {
        context->postTask([](auto& context) {
            context.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "A VideoFrame was destroyed without having been closed explicitly"_s);
        });
    }
}

// https://html.spec.whatwg.org/multipage/canvas.html#check-the-usability-of-the-image-argument
static std::optional<Exception> checkImageUsability(ScriptExecutionContext& context, const WebCodecsVideoFrame::CanvasImageSource& source)
{
    return switchOn(source,
    [&] (const RefPtr<HTMLImageElement>& imageElement) -> std::optional<Exception> {
        if (!imageElement->originClean(*context.securityOrigin()))
            return Exception { ExceptionCode::SecurityError, "Image element is tainted"_s };

        RefPtr image = imageElement->cachedImage() ? imageElement->cachedImage()->image() : nullptr;
        if (!image)
            return Exception { ExceptionCode::InvalidStateError,  "Image element has no data"_s };
        if (!image->width() || !image->height())
            return Exception { ExceptionCode::InvalidStateError,  "Image element has a bad size"_s };
        return { };
    },
    [] (const RefPtr<SVGImageElement>& imageElement) -> std::optional<Exception> {
        if (imageElement->renderingTaintsOrigin())
            return Exception { ExceptionCode::SecurityError, "Image element is tainted"_s };

        RefPtr image = imageElement->cachedImage() ? imageElement->cachedImage()->image() : nullptr;
        if (!image)
            return Exception { ExceptionCode::InvalidStateError,  "Image element has no data"_s };
        if (!image->width() || !image->height())
            return Exception { ExceptionCode::InvalidStateError,  "Image element has a bad size"_s };
        return { };
    },
    [&] (const RefPtr<CSSStyleImageValue>& cssImage) -> std::optional<Exception> {
        UNUSED_PARAM(cssImage);
        ASSERT(!cssImage->isLoadedFromOpaqueSource());
        return Exception { ExceptionCode::SecurityError, "Image element is tainted"_s };
    },
#if ENABLE(VIDEO)
    [&] (const RefPtr<HTMLVideoElement>& video) -> std::optional<Exception> {
        RefPtr origin = context.securityOrigin();
        if (video->taintsOrigin(*origin))
            return Exception { ExceptionCode::SecurityError, "Video element is tainted"_s };

        auto readyState = video->readyState();
        if (readyState < HTMLMediaElement::HAVE_CURRENT_DATA)
            return Exception { ExceptionCode::InvalidStateError,  "Video element has no data"_s };
        return { };
    },
#endif
    [] (const RefPtr<HTMLCanvasElement>& canvas) -> std::optional<Exception> {
        if (!canvas->originClean())
            return Exception { ExceptionCode::SecurityError, "Image element is tainted"_s };

        auto size = canvas->size();
        if (!size.width() || !size.height())
            return Exception { ExceptionCode::InvalidStateError,  "Input canvas has a bad size"_s };
        return { };
    },
#if ENABLE(OFFSCREEN_CANVAS)
    [] (const RefPtr<OffscreenCanvas>& canvas) -> std::optional<Exception> {
        if (!canvas->originClean())
            return Exception { ExceptionCode::SecurityError, "Image element is tainted"_s };

        if (!canvas->width() || !canvas->height())
            return Exception { ExceptionCode::InvalidStateError,  "Input canvas has a bad size"_s };
        return { };
    },
#endif
    [] (const RefPtr<ImageBitmap>& image) -> std::optional<Exception> {
        if (image->isDetached())
            return Exception { ExceptionCode::InvalidStateError,  "Input ImageBitmap is detached"_s };

        if (!image->originClean())
            return Exception { ExceptionCode::SecurityError,  "Input ImageBitmap is tainted"_s };
        return { };
    });
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(ScriptExecutionContext& context, CanvasImageSource&& source, Init&& init)
{
    if (auto exception = checkImageUsability(context, source))
        return WTFMove(*exception);

    return switchOn(source,
    [&] (RefPtr<HTMLImageElement>& imageElement) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { ExceptionCode::TypeError,  "timestamp is not provided"_s };

        auto image = imageElement->cachedImage()->image()->currentNativeImage();
        if (!image)
            return Exception { ExceptionCode::InvalidStateError,  "Image element has no video frame"_s };

        return initializeFrameWithResourceAndSize(context, image.releaseNonNull(), WTFMove(init));
    },
    [&] (RefPtr<SVGImageElement>& imageElement) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { ExceptionCode::TypeError,  "timestamp is not provided"_s };

        auto image = imageElement->cachedImage()->image()->currentNativeImage();
        if (!image)
            return Exception { ExceptionCode::InvalidStateError,  "Image element has no video frame"_s };

        return initializeFrameWithResourceAndSize(context, image.releaseNonNull(), WTFMove(init));
    },
    [&] (RefPtr<CSSStyleImageValue>& cssImage) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { ExceptionCode::TypeError,  "timestamp is not provided"_s };

        auto image = cssImage->image()->image()->currentNativeImage();
        if (!image)
            return Exception { ExceptionCode::InvalidStateError,  "CSS Image has no video frame"_s };

        return initializeFrameWithResourceAndSize(context, image.releaseNonNull(), WTFMove(init));
    },
#if ENABLE(VIDEO)
    [&] (RefPtr<HTMLVideoElement>& video) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        RefPtr videoFrame = video->player() ? video->player()->videoFrameForCurrentTime() : nullptr;
        if (!videoFrame)
            return Exception { ExceptionCode::InvalidStateError,  "Video element has no video frame"_s };
        return initializeFrameFromOtherFrame(context, videoFrame.releaseNonNull(), WTFMove(init), VideoFrame::ShouldCloneWithDifferentTimestamp::No);
    },
#endif
    [&] (RefPtr<HTMLCanvasElement>& canvas) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { ExceptionCode::TypeError,  "timestamp is not provided"_s };

        if (!canvas->width() || !canvas->height())
            return Exception { ExceptionCode::InvalidStateError,  "Input canvas has a bad size"_s };

        auto videoFrame = canvas->toVideoFrame();
        if (!videoFrame)
            return Exception { ExceptionCode::InvalidStateError,  "Canvas has no frame"_s };
        return initializeFrameFromOtherFrame(context, videoFrame.releaseNonNull(), WTFMove(init), VideoFrame::ShouldCloneWithDifferentTimestamp::Yes);
    },
#if ENABLE(OFFSCREEN_CANVAS)
    [&] (RefPtr<OffscreenCanvas>& canvas) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { ExceptionCode::TypeError,  "timestamp is not provided"_s };

        if (!canvas->width() || !canvas->height())
            return Exception { ExceptionCode::InvalidStateError,  "Input canvas has a bad size"_s };

        RefPtr imageBuffer = canvas->makeRenderingResultsAvailable();
        if (!imageBuffer)
            return Exception { ExceptionCode::InvalidStateError,  "Input canvas has no image buffer"_s };

        return create(context, *imageBuffer, { static_cast<int>(canvas->width()), static_cast<int>(canvas->height()) }, WTFMove(init));
    },
#endif // ENABLE(OFFSCREEN_CANVAS)
    [&] (RefPtr<ImageBitmap>& image) -> ExceptionOr<Ref<WebCodecsVideoFrame>> {
        if (!init.timestamp)
            return Exception { ExceptionCode::TypeError,  "timestamp is not provided"_s };

        if (!image->width() || !image->height())
            return Exception { ExceptionCode::InvalidStateError,  "Input image has a bad size"_s };

        RefPtr imageBuffer = image->buffer();
        if (!imageBuffer)
            return Exception { ExceptionCode::InvalidStateError,  "Input image has no image buffer"_s };

        return create(context, *imageBuffer, { static_cast<int>(image->width()), static_cast<int>(image->height()) }, WTFMove(init));
    });
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(ScriptExecutionContext& context, ImageBuffer& buffer, IntSize size, WebCodecsVideoFrame::Init&& init)
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    IntRect region { IntPoint::zero(), size };

    auto pixelBuffer = buffer.getPixelBuffer(format, region);
    if (!pixelBuffer)
        return Exception { ExceptionCode::InvalidStateError,  "Buffer has no frame"_s };

    auto videoFrame = VideoFrame::createFromPixelBuffer(pixelBuffer.releaseNonNull(), { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Iec6196621, PlatformVideoMatrixCoefficients::Rgb, true });

    if (!videoFrame)
        return Exception { ExceptionCode::InvalidStateError,  "Unable to create frame from buffer"_s };

    return WebCodecsVideoFrame::initializeFrameFromOtherFrame(context, videoFrame.releaseNonNull(), WTFMove(init), VideoFrame::ShouldCloneWithDifferentTimestamp::Yes);
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(ScriptExecutionContext& context, Ref<WebCodecsVideoFrame>&& initFrame, Init&& init)
{
    if (initFrame->isDetached())
        return Exception { ExceptionCode::InvalidStateError,  "VideoFrame is detached"_s };
    return initializeFrameFromOtherFrame(context, WTFMove(initFrame), WTFMove(init), VideoFrame::ShouldCloneWithDifferentTimestamp::Yes);
}

static std::optional<Exception> validateI420Sizes(const WebCodecsVideoFrame::BufferInit& init)
{
    if (init.codedWidth % 2 || init.codedHeight % 2)
        return Exception { ExceptionCode::TypeError, "coded width or height is odd"_s };
    if (init.visibleRect && (static_cast<size_t>(init.visibleRect->x) % 2 || static_cast<size_t>(init.visibleRect->x) % 2))
        return Exception { ExceptionCode::TypeError, "visible x or y is odd"_s };
    return { };
}

// https://w3c.github.io/webcodecs/#dom-videoframe-videoframe-data-init
ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(ScriptExecutionContext& context, BufferSource&& data, BufferInit&& init)
{
    if (!isValidVideoFrameBufferInit(init))
        return Exception { ExceptionCode::TypeError, "buffer init is not valid"_s };

    DOMRectInit defaultRect { 0, 0, static_cast<double>(init.codedWidth), static_cast<double>(init.codedHeight) };
    auto parsedRectOrExtension = parseVisibleRect(defaultRect, init.visibleRect, init.codedWidth, init.codedHeight, init.format);
    if (parsedRectOrExtension.hasException())
        return parsedRectOrExtension.releaseException();

    auto parsedRect = parsedRectOrExtension.releaseReturnValue();
    auto layoutOrException = computeLayoutAndAllocationSize(defaultRect, init.layout, init.format);
    if (layoutOrException.hasException())
        return layoutOrException.releaseException();
    
    auto layout = layoutOrException.releaseReturnValue();
    if (data.length() < layout.allocationSize)
        return Exception { ExceptionCode::TypeError, makeString("Data is too small "_s, data.length(), " / "_s, layout.allocationSize) };

    auto colorSpace = videoFramePickColorSpace(init.colorSpace, init.format);
    RefPtr<VideoFrame> videoFrame;
    if (init.format == VideoPixelFormat::NV12) {
        if (init.codedWidth % 2 || init.codedHeight % 2)
            return Exception { ExceptionCode::TypeError, "coded width or height is odd"_s };
        if (init.visibleRect && (static_cast<size_t>(init.visibleRect->x) % 2 || static_cast<size_t>(init.visibleRect->x) % 2))
            return Exception { ExceptionCode::TypeError, "visible x or y is odd"_s };
        videoFrame = VideoFrame::createNV12(data.span(), parsedRect.width, parsedRect.height, layout.computedLayouts[0], layout.computedLayouts[1], WTFMove(colorSpace));
    } else if (init.format == VideoPixelFormat::RGBA || init.format == VideoPixelFormat::RGBX)
        videoFrame = VideoFrame::createRGBA(data.span(), parsedRect.width, parsedRect.height, layout.computedLayouts[0], WTFMove(colorSpace));
    else if (init.format == VideoPixelFormat::BGRA || init.format == VideoPixelFormat::BGRX)
        videoFrame = VideoFrame::createBGRA(data.span(), parsedRect.width, parsedRect.height, layout.computedLayouts[0], WTFMove(colorSpace));
    else if (init.format == VideoPixelFormat::I420) {
        if (auto exception = validateI420Sizes(init))
            return WTFMove(*exception);
        videoFrame = VideoFrame::createI420(data.span(), parsedRect.width, parsedRect.height, layout.computedLayouts[0], layout.computedLayouts[1], layout.computedLayouts[2], WTFMove(colorSpace));
    } else if (init.format == VideoPixelFormat::I420A) {
        if (auto exception = validateI420Sizes(init))
            return WTFMove(*exception);
        videoFrame = VideoFrame::createI420A(data.span(), parsedRect.width, parsedRect.height, layout.computedLayouts[0], layout.computedLayouts[1], layout.computedLayouts[2], layout.computedLayouts[3], WTFMove(colorSpace));
    } else
        return Exception { ExceptionCode::NotSupportedError, "VideoPixelFormat is not supported"_s };

    if (!videoFrame)
        return Exception { ExceptionCode::TypeError, "Unable to create internal resource from data"_s };

    return WebCodecsVideoFrame::create(context, videoFrame.releaseNonNull(), WTFMove(init));
}

Ref<WebCodecsVideoFrame> WebCodecsVideoFrame::create(ScriptExecutionContext& context, Ref<VideoFrame>&& videoFrame, BufferInit&& init)
{
    ASSERT(isValidVideoFrameBufferInit(init));

    auto result = adoptRef(*new WebCodecsVideoFrame(context));
    result->m_data.internalFrame = WTFMove(videoFrame);
    result->m_data.format = init.format;

    result->m_data.codedWidth = result->m_data.internalFrame->presentationSize().width();
    result->m_data.codedHeight = result->m_data.internalFrame->presentationSize().height();

    result->m_data.visibleLeft = 0;
    result->m_data.visibleTop = 0;

    if (init.visibleRect) {
        result->m_data.visibleWidth = init.visibleRect->width;
        result->m_data.visibleHeight = init.visibleRect->height;
    } else {
        result->m_data.visibleWidth = result->m_data.codedWidth;
        result->m_data.visibleHeight = result->m_data.codedHeight;
    }

    result->m_data.displayWidth = init.displayWidth.value_or(result->m_data.visibleWidth);
    result->m_data.displayHeight = init.displayHeight.value_or(result->m_data.visibleHeight);

    result->m_data.duration = init.duration;
    result->m_data.internalFrame = result->m_data.internalFrame->updateTimestamp(timestampToMediaTime(init.timestamp), VideoFrame::ShouldCloneWithDifferentTimestamp::No);
    result->m_data.timestamp = init.timestamp;

    return result;
}

static VideoPixelFormat computeVideoPixelFormat(VideoPixelFormat baseFormat, bool shouldDiscardAlpha)
{
    if (!shouldDiscardAlpha)
        return baseFormat;
    switch (baseFormat) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I422:
    case VideoPixelFormat::NV12:
    case VideoPixelFormat::I444:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRX:
        return baseFormat;
    case VideoPixelFormat::I420A:
        return VideoPixelFormat::I420;
    case VideoPixelFormat::RGBA:
        return VideoPixelFormat::RGBX;
    case VideoPixelFormat::BGRA:
        return VideoPixelFormat::BGRX;
    }
    return baseFormat;
}

// https://w3c.github.io/webcodecs/#videoframe-initialize-frame-from-other-frame
ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::initializeFrameFromOtherFrame(ScriptExecutionContext& context, Ref<WebCodecsVideoFrame>&& videoFrame, Init&& init, VideoFrame::ShouldCloneWithDifferentTimestamp shouldCloneWithDifferentTimestamp)
{
    auto codedWidth = videoFrame->m_data.codedWidth;
    auto codedHeight = videoFrame->m_data.codedHeight;
    auto format = computeVideoPixelFormat(videoFrame->m_data.format.value_or(VideoPixelFormat::I420), init.alpha == WebCodecsAlphaOption::Discard);
    if (!validateVideoFrameInit(init, codedWidth, codedHeight, format))
        return Exception { ExceptionCode::TypeError,  "VideoFrameInit is not valid"_s };

    auto result = adoptRef(*new WebCodecsVideoFrame(context));
    result->m_data.internalFrame = videoFrame->m_data.internalFrame;
    if (videoFrame->m_data.format)
        result->m_data.format = format;

    result->m_data.codedWidth = videoFrame->m_data.codedWidth;
    result->m_data.codedHeight = videoFrame->m_data.codedHeight;

    initializeVisibleRectAndDisplaySize(result.get(), init, DOMRectInit { static_cast<double>(videoFrame->m_data.visibleLeft), static_cast<double>(videoFrame->m_data.visibleTop), static_cast<double>(videoFrame->m_data.visibleWidth), static_cast<double>(videoFrame->m_data.visibleHeight) }, videoFrame->m_data.displayWidth, videoFrame->m_data.displayHeight);

    result->m_data.duration = init.duration ? init.duration : videoFrame->m_data.duration;
    if (init.timestamp)
        result->m_data.internalFrame = result->m_data.internalFrame->updateTimestamp(timestampToMediaTime(*init.timestamp), shouldCloneWithDifferentTimestamp);
    result->m_data.timestamp = mediaTimeToTimestamp(result->m_data.internalFrame->presentationTime());

    return result;
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::initializeFrameFromOtherFrame(ScriptExecutionContext& context, Ref<VideoFrame>&& internalVideoFrame, Init&& init, VideoFrame::ShouldCloneWithDifferentTimestamp shouldCloneWithDifferentTimestamp)
{
    auto codedWidth = internalVideoFrame->presentationSize().width();
    auto codedHeight = internalVideoFrame->presentationSize().height();
    auto format = convertVideoFramePixelFormat(internalVideoFrame->pixelFormat(), init.alpha == WebCodecsAlphaOption::Discard);
    if (!validateVideoFrameInit(init, codedWidth, codedHeight, format))
        return Exception { ExceptionCode::TypeError,  "VideoFrameInit is not valid"_s };

    auto result = adoptRef(*new WebCodecsVideoFrame(context));
    result->m_data.internalFrame = WTFMove(internalVideoFrame);
    result->m_data.format = format;
    result->m_data.codedWidth = codedWidth;
    result->m_data.codedHeight = codedHeight;

    initializeVisibleRectAndDisplaySize(result.get(), init, DOMRectInit { 0, 0 , static_cast<double>(result->m_data.codedWidth), static_cast<double>(result->m_data.codedHeight) }, result->m_data.codedWidth, result->m_data.codedHeight);

    result->m_data.duration = init.duration;
    if (init.timestamp)
        result->m_data.internalFrame = result->m_data.internalFrame->updateTimestamp(timestampToMediaTime(*init.timestamp), shouldCloneWithDifferentTimestamp);
    result->m_data.timestamp = mediaTimeToTimestamp(result->m_data.internalFrame->presentationTime());

    return result;
}

// https://w3c.github.io/webcodecs/#videoframe-initialize-frame-with-resource-and-size
ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::initializeFrameWithResourceAndSize(ScriptExecutionContext& context, Ref<NativeImage>&& image, Init&& init)
{
    auto internalVideoFrame = VideoFrame::fromNativeImage(image.get());
    if (!internalVideoFrame)
        return Exception { ExceptionCode::TypeError,  "image has no resource"_s };

    auto codedWidth = image->size().width();
    auto codedHeight = image->size().height();
    auto format = convertVideoFramePixelFormat(internalVideoFrame->pixelFormat(), init.alpha == WebCodecsAlphaOption::Discard);
    if (!validateVideoFrameInit(init, codedWidth, codedHeight, format))
        return Exception { ExceptionCode::TypeError,  "VideoFrameInit is not valid"_s };

    auto result = adoptRef(*new WebCodecsVideoFrame(context));
    result->m_data.internalFrame = WTFMove(internalVideoFrame);
    result->m_data.format = format;
    result->m_data.codedWidth = codedWidth;
    result->m_data.codedHeight = codedHeight;

    initializeVisibleRectAndDisplaySize(result.get(), init, DOMRectInit { 0, 0 , static_cast<double>(result->m_data.codedWidth), static_cast<double>(result->m_data.codedHeight) }, result->m_data.codedWidth, result->m_data.codedHeight);

    result->m_data.duration = init.duration;
    if (init.timestamp)
        result->m_data.internalFrame = result->m_data.internalFrame->updateTimestamp(timestampToMediaTime(*init.timestamp), VideoFrame::ShouldCloneWithDifferentTimestamp::No);
    result->m_data.timestamp = mediaTimeToTimestamp(result->m_data.internalFrame->presentationTime());

    return result;
}


ExceptionOr<size_t> WebCodecsVideoFrame::allocationSize(const CopyToOptions& options)
{
    if (isDetached())
        return Exception { ExceptionCode::InvalidStateError,  "VideoFrame is detached"_s };

    if (!m_data.format)
        return Exception { ExceptionCode::NotSupportedError,  "VideoFrame has no format"_s };

    auto layoutOrException = parseVideoFrameCopyToOptions(*this, options);
    if (layoutOrException.hasException())
        return layoutOrException.releaseException();

    return layoutOrException.returnValue().allocationSize;
}

void WebCodecsVideoFrame::copyTo(BufferSource&& source, CopyToOptions&& options, CopyToPromise&& promise)
{
    if (isDetached()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError,  "VideoFrame is detached"_s });
        return;
    }
    if (!m_data.format) {
        promise.reject(Exception { ExceptionCode::NotSupportedError,  "VideoFrame has no format"_s });
        return;
    }

    auto combinedLayoutOrException = parseVideoFrameCopyToOptions(*this, options);
    if (combinedLayoutOrException.hasException()) {
        promise.reject(combinedLayoutOrException.releaseException());
        return;
    }

    auto combinedLayout = combinedLayoutOrException.releaseReturnValue();
    if (source.length() < combinedLayout.allocationSize) {
        promise.reject(Exception { ExceptionCode::TypeError,  "Buffer is too small"_s });
        return;
    }

    auto buffer = source.mutableSpan();
    m_data.internalFrame->copyTo(buffer, *m_data.format, WTFMove(combinedLayout.computedLayouts), [source = WTFMove(source), promise = WTFMove(promise)](auto planeLayouts) mutable {
        if (!planeLayouts) {
            promise.reject(Exception { ExceptionCode::TypeError,  "Unable to copy data"_s });
            return;
        }
        promise.resolve(WTFMove(*planeLayouts));
    });
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::clone(ScriptExecutionContext& context)
{
    if (isDetached())
        return Exception { ExceptionCode::InvalidStateError,  "VideoFrame is detached"_s };

    auto clone = adoptRef(*new WebCodecsVideoFrame(context, WebCodecsVideoFrameData { m_data }));

    clone->m_colorSpace = &colorSpace();
    clone->m_codedRect = codedRect();
    clone->m_visibleRect = visibleRect();
    clone->m_isDetached = m_isDetached;

    return clone;
}

// https://w3c.github.io/webcodecs/#close-videoframe
void WebCodecsVideoFrame::close()
{
    m_data.internalFrame = nullptr;

    m_isDetached = true;

    m_data.format = { };

    m_data.codedWidth = 0;
    m_data.codedHeight = 0;
    m_data.displayWidth = 0;
    m_data.displayHeight = 0;
    m_data.visibleWidth = 0;
    m_data.visibleHeight = 0;
    m_data.visibleLeft = 0;
    m_data.visibleTop = 0;

    m_codedRect = nullptr;
    m_visibleRect = nullptr;
}

DOMRectReadOnly* WebCodecsVideoFrame::codedRect() const
{
    if (m_isDetached)
        return nullptr;
    if (!m_codedRect)
        m_codedRect = DOMRectReadOnly::create(0, 0, m_data.codedWidth, m_data.codedHeight);

    return m_codedRect.get();
}

DOMRectReadOnly* WebCodecsVideoFrame::visibleRect() const
{
    if (m_isDetached)
        return nullptr;
    if (!m_visibleRect)
        m_visibleRect = DOMRectReadOnly::create(m_data.visibleLeft, m_data.visibleTop, m_data.visibleWidth, m_data.visibleHeight);

    return m_visibleRect.get();
}

void WebCodecsVideoFrame::setDisplaySize(size_t width, size_t height)
{
    m_data.displayWidth = width;
    m_data.displayHeight = height;
}

void WebCodecsVideoFrame::setVisibleRect(const DOMRectInit& rect)
{
    m_data.visibleLeft = rect.x;
    m_data.visibleTop = rect.y;
    m_data.visibleWidth = rect.width;
    m_data.visibleHeight = rect.height;
}

VideoColorSpace& WebCodecsVideoFrame::colorSpace() const
{
    if (!m_colorSpace)
        m_colorSpace = m_data.internalFrame ? VideoColorSpace::create(m_data.internalFrame->colorSpace()) : VideoColorSpace::create();

    return *m_colorSpace.get();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
