/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ImageCapture.h"

#if ENABLE(MEDIA_STREAM)

#include "CanvasCaptureMediaStreamTrack.h"
#include "ContextDestructionObserverInlines.h"
#include "GraphicsContext.h"
#include "ImageBitmapOptions.h"
#include "ImageBuffer.h"
#include "JSBlob.h"
#include "JSImageBitmap.h"
#include "JSPhotoCapabilities.h"
#include "JSPhotoSettings.h"
#include "Logging.h"
#include "MediaStrategy.h"
#include "PlatformStrategies.h"
#include "TaskSource.h"
#include "VideoFrame.h"
#include <wtf/Compiler.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ImageCapture);

ExceptionOr<Ref<ImageCapture>> ImageCapture::create(Document& document, Ref<MediaStreamTrack> track)
{
    if (track->kind() != "video"_s)
        return Exception { ExceptionCode::NotSupportedError, "Invalid track kind"_s };

    auto imageCapture = adoptRef(*new ImageCapture(document, track));
    imageCapture->suspendIfNeeded();
    return imageCapture;
}

ImageCapture::ImageCapture(Document& document, Ref<MediaStreamTrack> track)
    : ActiveDOMObject(document)
    , m_track(track)
#if !RELEASE_LOG_DISABLED
    , m_logger(track->logger())
    , m_logIdentifier(track->logIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

ImageCapture::~ImageCapture()
{
    stop();
}

void ImageCapture::takePhoto(PhotoSettings&& settings, DOMPromiseDeferred<IDLInterface<Blob>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    // https://w3c.github.io/mediacapture-image/#dom-imagecapture-takephoto
    // If the readyState of track provided in the constructor is not live, return
    // a promise rejected with a new DOMException whose name is InvalidStateError,
    // and abort these steps.
    if (m_track->readyState() == MediaStreamTrack::State::Ended) {
        ERROR_LOG(identifier, "rejecting promise, track has ended");
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });
        return;
    }

    m_track->takePhoto(WTFMove(settings))->whenSettled(RunLoop::mainSingleton(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::ImageCapture, [promise = WTFMove(promise), result = WTFMove(result), identifier = WTFMove(identifier)](ImageCapture& capture) mutable {
            if (!result) {
                ERROR_LOG_WITH_THIS(&capture, identifier, "rejecting promise: ", result.error().message());
                promise.reject(WTFMove(result.error()));
                return;
            }

            ALWAYS_LOG_WITH_THIS(&capture, identifier, "resolving promise");

            // FIXME: This is a static analysis false positive (rdar://146889777).
            SUPPRESS_UNCOUNTED_ARG promise.resolve(Blob::create(capture.scriptExecutionContext(), WTFMove(get<0>(result.value())), WTFMove(get<1>(result.value()))));
        });
    });
}

// FIXME: Move this routine to VideoFrame.
static ImageOrientation videoFrameOrientation(const VideoFrame& videoFrame)
{
    switch (videoFrame.rotation()) {
    case VideoFrame::Rotation::None:
        return videoFrame.isMirrored() ? ImageOrientation::Orientation::OriginTopRight : ImageOrientation::Orientation::OriginTopLeft;
    case VideoFrame::Rotation::Right:
        return videoFrame.isMirrored() ? ImageOrientation::Orientation::OriginRightBottom : ImageOrientation::Orientation::OriginRightTop;
    case VideoFrame::Rotation::UpsideDown:
        return videoFrame.isMirrored() ? ImageOrientation::Orientation::OriginBottomLeft : ImageOrientation::Orientation::OriginBottomRight;
    case VideoFrame::Rotation::Left:
        return videoFrame.isMirrored() ? ImageOrientation::Orientation::OriginLeftTop : ImageOrientation::Orientation::OriginLeftBottom;
    }
    ASSERT_NOT_REACHED();
    return ImageOrientation::Orientation::OriginTopLeft;
}

static void createImageBitmap(VideoFrame& videoFrame, CompletionHandler<void(RefPtr<ImageBitmap>&&)>&& completionHandler)
{
    auto size = videoFrame.presentationSize();
    if (videoFrame.has90DegreeRotation())
        size = size.transposedSize();
    RefPtr imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer) {
        completionHandler({ });
        return;
    }
    RefPtr image = videoFrame.copyNativeImage();
    if (!image) {
        completionHandler({ });
        return;
    }
    imageBuffer->context().drawNativeImage(*image, { { }, size }, { { }, size }, { videoFrameOrientation(videoFrame), CompositeOperator::Copy });
    bool isOriginClean = true;
    completionHandler(ImageBitmap::create(imageBuffer.releaseNonNull(), isOriginClean));
}

static Exception createImageCaptureException()
{
    return Exception { WebCore::ExceptionCode::UnknownError, "Unable to create ImageBitmap"_s };
}

static void createImageBitmapOrException(VideoFrame& videoFrame, CompletionHandler<void(ExceptionOr<Ref<ImageBitmap>>&&)>&& callback)
{
    createImageBitmap(videoFrame, [callback = WTFMove(callback)](auto&& bitmap) mutable {
        if (!bitmap) {
            callback(createImageCaptureException());
            return;
        }
        callback(bitmap.releaseNonNull());
    });
}

class ImageCaptureVideoFrameObserver : public RealtimeMediaSource::VideoFrameObserver, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ImageCaptureVideoFrameObserver, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<ImageCaptureVideoFrameObserver> create(Ref<RealtimeMediaSource>&& source) { return adoptRef(*new ImageCaptureVideoFrameObserver(WTFMove(source))); }
    ~ImageCaptureVideoFrameObserver()
    {
        ASSERT(m_callbacks.isEmpty());
    }

    using Callback = CompletionHandler<void(ExceptionOr<Ref<ImageBitmap>>&&)>;
    void add(Callback&& callback)
    {
        if (m_callbacks.isEmpty())
            m_source->addVideoFrameObserver(*this);

        m_callbacks.append(WTFMove(callback));
    }

    void stop()
    {
        if (!m_callbacks.isEmpty())
            m_source->removeVideoFrameObserver(*this);

        while (!m_callbacks.isEmpty())
            m_callbacks.takeFirst()(WebCore::Exception { WebCore::ExceptionCode::OperationError, "grabFrame is stopped"_s });
    }

private:
    explicit ImageCaptureVideoFrameObserver(Ref<RealtimeMediaSource>&& source)
        : m_source(WTFMove(source))
    {
    }

    void videoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata) final
    {
        {
            Locker locker(m_frameLock);
            m_frame = frame;
        }
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            RefPtr<VideoFrame> frame;
            {
                Locker locker(protectedThis->m_frameLock);
                frame = std::exchange(protectedThis->m_frame, { });
            }

            if (!frame)
                return;

            protectedThis->processVideoFrame(*frame);
        });
    }

    void processVideoFrame(VideoFrame& frame)
    {
        ASSERT(isMainThread());
        if (m_callbacks.isEmpty())
            return;

        createImageBitmapOrException(frame, m_callbacks.takeFirst());

        if (m_callbacks.isEmpty())
            m_source->removeVideoFrameObserver(*this);
    }

    Deque<Callback> m_callbacks;
    const Ref<RealtimeMediaSource> m_source;
    Lock m_frameLock;
    RefPtr<VideoFrame> m_frame WTF_GUARDED_BY_LOCK(m_frameLock);
};

void ImageCapture::grabFrame(DOMPromiseDeferred<IDLInterface<ImageBitmap>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (m_track->readyState() == MediaStreamTrack::State::Ended) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });
        return;
    }

    if (RefPtr canvasTrack = dynamicDowncast<CanvasCaptureMediaStreamTrack>(m_track.get())) {
        ImageCaptureVideoFrameObserver::Callback callback = [promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](auto&& result) mutable {
            if (pendingActivity->object().isContextStopped())
                return;

            queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::ImageCapture, [promise = WTFMove(promise), result = WTFMove(result)](auto&) mutable {
                promise.settle(WTFMove(result));
            });
        };
        callOnMainThread([frame = canvasTrack->grabFrame(), callback = WTFMove(callback)]() mutable {
            if (!frame) {
                callback(createImageCaptureException());
                return;
            }

            createImageBitmapOrException(*frame, WTFMove(callback));
        });
        return;
    }

    if (!m_grabFrameObserver) {
        m_grabFrameObserver = ImageCaptureVideoFrameObserver::create(m_track->source());
        Ref { m_track->privateTrack() }->addObserver(*this);
    }

    Ref { *m_grabFrameObserver }->add([promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](auto&& result) mutable {
        queueTaskKeepingObjectAlive(pendingActivity->object(), TaskSource::ImageCapture, [promise = WTFMove(promise), result = WTFMove(result)](auto&) mutable {
            promise.settle(WTFMove(result));
        });
    });
}

void ImageCapture::stopGrabFrameObserver()
{
    if (auto grabFrameObserver = std::exchange(m_grabFrameObserver, { })) {
        Ref { m_track->privateTrack() }->removeObserver(*this);
        grabFrameObserver->stop();
    }
}

void ImageCapture::trackEnded(MediaStreamTrackPrivate&)
{
    callOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->stopGrabFrameObserver();
    });
}

void ImageCapture::getPhotoCapabilities(DOMPromiseDeferred<IDLDictionary<PhotoCapabilities>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    // https://w3c.github.io/mediacapture-image/#dom-imagecapture-getphotocapabilities
    // If the readyState of track provided in the constructor is not live, return
    // a promise rejected with a new DOMException whose name is InvalidStateError,
    // and abort these steps.
    if (m_track->readyState() == MediaStreamTrack::State::Ended) {
        ERROR_LOG(identifier, "rejecting promise, track has ended");
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });
        return;
    }

    m_track->getPhotoCapabilities()->whenSettled(RunLoop::mainSingleton(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::ImageCapture, [promise = WTFMove(promise), result = WTFMove(result), identifier = WTFMove(identifier)](auto& capture) mutable {
            if (!result) {
                ERROR_LOG_WITH_THIS(&capture, identifier, "rejecting promise: ", result.error().message());
                promise.reject(WTFMove(result.error()));
                return;
            }

            ALWAYS_LOG_WITH_THIS(&capture, identifier, "resolving promise");
            promise.resolve(WTFMove(result.value()));
        });
    });
}

void ImageCapture::getPhotoSettings(DOMPromiseDeferred<IDLDictionary<PhotoSettings>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    // https://w3c.github.io/mediacapture-image/#ref-for-dom-imagecapture-getphotosettings②
    // If the readyState of track provided in the constructor is not live, return
    // a promise rejected with a new DOMException whose name is InvalidStateError,
    // and abort these steps.
    if (m_track->readyState() == MediaStreamTrack::State::Ended) {
        ERROR_LOG(identifier, "rejecting promise, track has ended");
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });
        return;
    }

    m_track->getPhotoSettings()->whenSettled(RunLoop::mainSingleton(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::ImageCapture, [promise = WTFMove(promise), result = WTFMove(result), identifier = WTFMove(identifier)](auto& capture) mutable {
            if (!result) {
                ERROR_LOG_WITH_THIS(&capture, identifier, "rejecting promise: ", result.error().message());
                promise.reject(WTFMove(result.error()));
                return;
            }

            ALWAYS_LOG_WITH_THIS(&capture, identifier, "resolving promise");
            promise.resolve(WTFMove(result.value()));
        });
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& ImageCapture::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif
