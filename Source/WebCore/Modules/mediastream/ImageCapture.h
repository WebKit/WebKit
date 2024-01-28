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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "Blob.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "MediaStreamTrack.h"
#include "PhotoCapabilities.h"
#include "PhotoSettings.h"

namespace WTF {
class Logger;
}

namespace WebCore {

class ImageCapture : public RefCounted<ImageCapture>, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(ImageCapture);
public:
    static ExceptionOr<Ref<ImageCapture>> create(Document&, Ref<MediaStreamTrack>);

    ~ImageCapture();

    void takePhoto(PhotoSettings&&, DOMPromiseDeferred<IDLInterface<Blob>>&&);
    void getPhotoCapabilities(DOMPromiseDeferred<IDLDictionary<PhotoCapabilities>>&&);
    void getPhotoSettings(DOMPromiseDeferred<IDLDictionary<PhotoSettings>>&&);

    Ref<MediaStreamTrack> track() const { return m_track; }

private:
    ImageCapture(Document&, Ref<MediaStreamTrack>);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger.get(); }
    const void* logIdentifier() const { return m_logIdentifier; }
    const char* logClassName() const { return "ImageCapture"; }
    WTFLogChannel& logChannel() const;
#endif

    // ActiveDOMObject API.
    const char* activeDOMObjectName() const final;

    Ref<MediaStreamTrack> m_track;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

}

#endif
