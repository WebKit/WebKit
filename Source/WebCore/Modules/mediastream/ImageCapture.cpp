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
#include "TaskSource.h"
#include <wtf/IsoMallocInlines.h>
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
{
}

ImageCapture::~ImageCapture() = default;

void ImageCapture::takePhoto(PhotoSettings&& settings, DOMPromiseDeferred<IDLInterface<Blob>>&& promise)
{
    m_track->takePhoto(WTFMove(settings))->whenSettled(RunLoop::main(), [protectedThis = Ref { *this }, promise = WTFMove(promise)] (auto&& result) mutable {
        queueTaskKeepingObjectAlive(protectedThis.get(), TaskSource::ImageCapture, [promise = WTFMove(promise), result = WTFMove(result), protectedThis] () mutable {
            if (result)
                promise.resolve(Blob::create(protectedThis->scriptExecutionContext(), WTFMove(get<0>(result.value())), WTFMove(get<1>(result.value()))));
            else
                promise.reject(WTFMove(result.error()));
        });
    });
}

void ImageCapture::getPhotoCapabilities(PhotoCapabilitiesPromise&& promise)
{
    if (m_track->readyState() == MediaStreamTrack::State::Ended) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });
        return;
    }

    m_track->getPhotoCapabilities(WTFMove(promise));
}

void ImageCapture::getPhotoSettings(PhotoSettingsPromise&& promise)
{
    if (m_track->readyState() == MediaStreamTrack::State::Ended) {
        // https://w3c.github.io/mediacapture-image/#ref-for-dom-imagecapture-getphotosettings②
        // If the readyState of track provided in the constructor is not live, return a promise
        // rejected with a new DOMException whose name is InvalidStateError, and abort these steps.
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });
        return;
    }

    m_track->getPhotoSettings(WTFMove(promise));
}

const char* ImageCapture::activeDOMObjectName() const
{
    return "ImageCapture";
}

} // namespace WebCore

#endif
