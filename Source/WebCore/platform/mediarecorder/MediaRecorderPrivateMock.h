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

#if ENABLE(MEDIA_STREAM)

#include "MediaRecorderPrivate.h"
#include <wtf/Lock.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class MediaStreamTrackPrivate;

class WEBCORE_EXPORT MediaRecorderPrivateMock final
    : public MediaRecorderPrivate {
public:
    explicit MediaRecorderPrivateMock(MediaStreamPrivate&);
    ~MediaRecorderPrivateMock();

private:
    // MediaRecorderPrivate
    void videoSampleAvailable(MediaSample&) final;
    void fetchData(FetchDataCallback&&) final;
    void audioSamplesAvailable(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;
    void stopRecording() final;
    const String& mimeType() const final;

    void generateMockCounterString();

    mutable Lock m_bufferLock;
    StringBuilder m_buffer;
    unsigned m_counter { 0 };
    String m_audioTrackID;
    String m_videoTrackID;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
