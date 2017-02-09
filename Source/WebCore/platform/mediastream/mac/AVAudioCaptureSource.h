/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#ifndef AVAudioCaptureSource_h
#define AVAudioCaptureSource_h

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "AVMediaCaptureSource.h"
#include "AudioCaptureSourceProviderObjC.h"
#include "CAAudioStreamDescription.h"
#include <wtf/Lock.h>

typedef struct AudioBufferList AudioBufferList;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class WebAudioBufferList;
class WebAudioSourceProviderAVFObjC;

class AVAudioCaptureSource : public AVMediaCaptureSource, public AudioCaptureSourceProviderObjC {
public:

    static RefPtr<AVMediaCaptureSource> create(AVCaptureDevice*, const AtomicString&, const MediaConstraints*, String&);

private:
    AVAudioCaptureSource(AVCaptureDevice*, const AtomicString&);
    virtual ~AVAudioCaptureSource();
    
    // AudioCaptureSourceProviderObjC
    void addObserver(AudioSourceObserverObjC&) final;
    void removeObserver(AudioSourceObserverObjC&) final;
    void start() final;

    void initializeCapabilities(RealtimeMediaSourceCapabilities&) override;
    void initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints&) override;

    void captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef, AVCaptureConnection*) override;
    
    void setupCaptureSession() override;
    void shutdownCaptureSession() override;
    void updateSettings(RealtimeMediaSourceSettings&) override;
    AudioSourceProvider* audioSourceProvider() override;

    RetainPtr<AVCaptureConnection> m_audioConnection;
    std::unique_ptr<WebAudioBufferList> m_list;

    RefPtr<WebAudioSourceProviderAVFObjC> m_audioSourceProvider;
    std::unique_ptr<CAAudioStreamDescription> m_inputDescription;
    Vector<AudioSourceObserverObjC*> m_observers;
    Lock m_lock;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // AVVideoCaptureSource_h
