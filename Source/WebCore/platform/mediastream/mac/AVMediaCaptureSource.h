/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef AVMediaCaptureSource_h
#define AVMediaCaptureSource_h

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "GenericTaskQueue.h"
#include "RealtimeMediaSource.h"
#include "Timer.h"
#include <wtf/Function.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVCaptureAudioDataOutput;
OBJC_CLASS AVCaptureConnection;
OBJC_CLASS AVCaptureDevice;
OBJC_CLASS AVCaptureOutput;
OBJC_CLASS AVCaptureSession;
OBJC_CLASS AVCaptureVideoDataOutput;
OBJC_CLASS WebCoreAVMediaCaptureSourceObserver;

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class AVMediaCaptureSource : public RealtimeMediaSource {
public:
    virtual ~AVMediaCaptureSource();

    virtual void captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef, AVCaptureConnection*) = 0;

    virtual void captureSessionIsRunningDidChange(bool);
    
    AVCaptureSession *session() const { return m_session.get(); }

    const RealtimeMediaSourceSettings& settings() override;

    void startProducingData() override;
    void stopProducingData() override;
    bool isProducingData() const override { return m_isRunning; }

    WeakPtr<AVMediaCaptureSource> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

protected:
    AVMediaCaptureSource(AVCaptureDevice*, const AtomicString&, RealtimeMediaSource::Type, PassRefPtr<MediaConstraints>);

    AudioSourceProvider* audioSourceProvider() override;

    virtual void setupCaptureSession() = 0;
    virtual void shutdownCaptureSession() = 0;
    virtual void updateSettings(RealtimeMediaSourceSettings&) = 0;
    virtual void initializeCapabilities(RealtimeMediaSourceCapabilities&) = 0;
    virtual void initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints&) = 0;

    AVCaptureDevice *device() const { return m_device.get(); }

    MediaConstraints* constraints() { return m_constraints.get(); }

    RealtimeMediaSourceSupportedConstraints& supportedConstraints();
    RefPtr<RealtimeMediaSourceCapabilities> capabilities() override;

    void setVideoSampleBufferDelegate(AVCaptureVideoDataOutput*);
    void setAudioSampleBufferDelegate(AVCaptureAudioDataOutput*);

    void scheduleDeferredTask(Function<void ()>&&);

private:
    void setupSession();
    void reset() override;

    RealtimeMediaSourceSettings m_currentSettings;
    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
    WeakPtrFactory<AVMediaCaptureSource> m_weakPtrFactory;
    RetainPtr<WebCoreAVMediaCaptureSourceObserver> m_objcObserver;
    RefPtr<MediaConstraints> m_constraints;
    RefPtr<RealtimeMediaSourceCapabilities> m_capabilities;
    RetainPtr<AVCaptureSession> m_session;
    RetainPtr<AVCaptureDevice> m_device;
    bool m_isRunning { false};
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // AVMediaCaptureSource_h
