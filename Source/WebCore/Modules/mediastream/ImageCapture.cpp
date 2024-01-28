/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "JSBlob.h"
#include "JSPhotoCapabilities.h"
#include "JSPhotoSettings.h"
#include "Logging.h"
#include "TaskSource.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RunLoop.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageCapture);

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

ImageCapture::~ImageCapture() = default;

void ImageCapture::takePhoto(PhotoSettings&& settings, DOMPromiseDeferred<IDLInterface<Blob>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    m_track->takePhoto(WTFMove(settings))->whenSettled(RunLoop::main(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::ImageCapture, [this, promise = WTFMove(promise), result = WTFMove(result), identifier = WTFMove(identifier)] () mutable {
            if (!result) {
                ERROR_LOG(identifier, "rejecting promise: ", result.error().message());
                promise.reject(WTFMove(result.error()));
                return;
            }

            ALWAYS_LOG(identifier, "resolving promise");
            promise.resolve(Blob::create(scriptExecutionContext(), WTFMove(get<0>(result.value())), WTFMove(get<1>(result.value()))));
        });
    });
}

void ImageCapture::getPhotoCapabilities(DOMPromiseDeferred<IDLDictionary<PhotoCapabilities>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    m_track->getPhotoCapabilities()->whenSettled(RunLoop::main(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::ImageCapture, [this, promise = WTFMove(promise), result = WTFMove(result), identifier = WTFMove(identifier)] () mutable {
            if (!result) {
                ERROR_LOG(identifier, "rejecting promise: ", result.error().message());
                promise.reject(WTFMove(result.error()));
                return;
            }

            ALWAYS_LOG(identifier, "resolving promise");
            promise.resolve(WTFMove(result.value()));
        });
    });
}

void ImageCapture::getPhotoSettings(DOMPromiseDeferred<IDLDictionary<PhotoSettings>>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    m_track->getPhotoSettings()->whenSettled(RunLoop::main(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise), identifier = WTFMove(identifier)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::ImageCapture, [this, promise = WTFMove(promise), result = WTFMove(result), identifier = WTFMove(identifier)] () mutable {
            if (!result) {
                ERROR_LOG(identifier, "rejecting promise: ", result.error().message());
                promise.reject(WTFMove(result.error()));
                return;
            }

            ALWAYS_LOG(identifier, "resolving promise");
            promise.resolve(WTFMove(result.value()));
        });
    });
}

const char* ImageCapture::activeDOMObjectName() const
{
    return "ImageCapture";
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& ImageCapture::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif
