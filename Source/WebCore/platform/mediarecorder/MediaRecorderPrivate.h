/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include "Exception.h"

#if ENABLE(MEDIA_STREAM)

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class MediaSample;
class MediaStreamTrackPrivate;
class PlatformAudioData;
class SharedBuffer;

class MediaRecorderPrivate {
public:
    virtual ~MediaRecorderPrivate() = default;

    virtual void sampleBufferUpdated(const MediaStreamTrackPrivate&, MediaSample&) = 0;
    virtual void audioSamplesAvailable(const MediaStreamTrackPrivate&, const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) = 0;

    virtual void fetchData(CompletionHandler<void(RefPtr<SharedBuffer>&&, const String& mimeType)>&&) = 0;
    virtual void stopRecording() { };

    using ErrorCallback = Function<void(Optional<Exception>&&)>;
    void setErrorCallback(ErrorCallback&& errorCallback) { m_errorCallback = WTFMove(errorCallback); }

protected:
    ErrorCallback m_errorCallback;
};
    
} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
