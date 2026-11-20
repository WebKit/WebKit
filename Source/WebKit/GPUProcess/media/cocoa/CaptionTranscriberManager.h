/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && HAVE(AVCAPTIONTRANSCRIBER)

#include "GPUProcess.h"
#include "RemoteAudioVideoRendererIdentifier.h"
#include <WebCore/MediaSessionIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVCaptionTranscriber;
OBJC_CLASS NSError;
OBJC_CLASS WKAVCaptionTranscriberDelegate;

namespace WebCore {
class AudioVideoRenderer;
class MediaSample;
struct GenericCueData;
}

namespace WebKit {

class CaptionTranscriberManager final : public GPUProcess::NowPlayingOwnerObserver, public CanMakeCheckedPtr<CaptionTranscriberManager> {
    WTF_MAKE_TZONE_ALLOCATED(CaptionTranscriberManager);
    WTF_MAKE_NONCOPYABLE(CaptionTranscriberManager);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CaptionTranscriberManager);
public:
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);

    explicit CaptionTranscriberManager(GPUProcess&);
    ~CaptionTranscriberManager();

    void rendererSessionsDidChange();

    void setMockEnabledForTesting(bool);


    void captionTranscriberDidOutputCaptionGroups(AVCaptionTranscriber *);
    void captionTranscriberDidFinish(AVCaptionTranscriber *, NSError *);

private:
    void nowPlayingOwnerDidChange(const std::optional<GPUProcess::NowPlayingOwner>&) final;

    void updateActiveSession();
    void activate(WebCore::QualifiedMediaSessionIdentifier, RemoteAudioVideoRendererIdentifier, const String& language, Ref<WebCore::AudioVideoRenderer>&&);
    enum class NotifyWebProcess : bool { No, Yes };
    void deactivate(NotifyWebProcess = NotifyWebProcess::Yes);
    bool createTranscriber(WebCore::AudioVideoRenderer&, const String& language);
    void sendAvailability(bool);

    void appendSample(const WebCore::MediaSample&);
    void appendMockSample(const WebCore::MediaSample&);
    void resetTranscriber();
    void deliverCues(Vector<WebCore::GenericCueData>&&);

    struct ActiveRenderer {
        WebCore::QualifiedMediaSessionIdentifier session;
        RemoteAudioVideoRendererIdentifier identifier;
        String language;
        ThreadSafeWeakPtr<WebCore::AudioVideoRenderer> renderer;
    };
    std::optional<WebCore::QualifiedMediaSessionIdentifier> m_ownerSession;
    std::optional<ActiveRenderer> m_active;

    bool m_mockEnabled { false };
    std::optional<int64_t> m_mockNextCueSecond;
    uint64_t m_audioSampleCount { 0 };

    RetainPtr<AVCaptionTranscriber> m_transcriber;
    RetainPtr<WKAVCaptionTranscriberDelegate> m_transcriberDelegate;
};

}

#endif
