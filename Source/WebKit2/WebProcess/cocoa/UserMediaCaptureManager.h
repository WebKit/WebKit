/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "MessageReceiver.h"
#include "SharedMemory.h"
#include "WebProcessSupplement.h"
#include <WebCore/RealtimeMediaSource.h>
#include <wtf/HashMap.h>

namespace WebCore {
class CAAudioStreamDescription;
}

namespace WebKit {

class CrossProcessRealtimeAudioSource;
class WebProcess;

class UserMediaCaptureManager : public WebProcessSupplement, public IPC::MessageReceiver, public WebCore::RealtimeMediaSource::CaptureFactory {
public:
    explicit UserMediaCaptureManager(WebProcess*);
    ~UserMediaCaptureManager();

    static const char* supplementName();

private:
    // WebProcessSupplement
    void initialize(const WebProcessCreationParameters&) final;

    // WebCore::RealtimeMediaSource::Factory:
    RefPtr<WebCore::RealtimeMediaSource> createMediaSourceForCaptureDeviceWithConstraints(const WebCore::CaptureDevice&, const WebCore::MediaConstraints*, String&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages::UserMediaCaptureManager
    void sourceStopped(uint64_t id);
    void sourceMutedChanged(uint64_t id, bool muted);
    void sourceEnabledChanged(uint64_t id, bool enabled);
    void sourceSettingsChanged(uint64_t id, const WebCore::RealtimeMediaSourceSettings&);
    void storageChanged(uint64_t id, const SharedMemory::Handle&, const WebCore::CAAudioStreamDescription&, uint64_t numberOfFrames);
    void ringBufferFrameBoundsChanged(uint64_t id, uint64_t startFrame, uint64_t endFrame);
    void audioSamplesAvailable(uint64_t id, MediaTime, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame);

    void startProducingData(uint64_t);
    void stopProducingData(uint64_t);

    class Source;
    friend class Source;
    HashMap<uint64_t, RefPtr<Source>> m_sources;
    WebProcess& m_process;
};

}

#endif
