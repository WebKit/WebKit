/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "MediaStreamSource.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS AVCaptureAudioDataOutput;
OBJC_CLASS AVCaptureConnection;
OBJC_CLASS AVCaptureDevice;
OBJC_CLASS AVCaptureOutput;
OBJC_CLASS AVCaptureSession;
OBJC_CLASS AVCaptureVideoDataOutput;
OBJC_CLASS WebCoreAVMediaCaptureSourceObserver;

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class AVMediaCaptureSource : public MediaStreamSource {
public:
    virtual ~AVMediaCaptureSource();

    virtual void captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef, AVCaptureConnection*) = 0;

    virtual void captureSessionStoppedRunning();

protected:
    AVMediaCaptureSource(AVCaptureDevice*, const AtomicString&, MediaStreamSource::Type, PassRefPtr<MediaConstraints>);

    virtual const MediaStreamSourceStates& states() override;

    virtual void startProducingData() override;
    virtual void stopProducingData() override;

    virtual void setupCaptureSession() = 0;
    virtual void updateStates() = 0;

    AVCaptureSession *session() const { return m_session.get(); }
    AVCaptureDevice *device() const { return m_device.get(); }
    MediaStreamSourceStates* currentStates() { return &m_currentStates; }
    MediaConstraints* constraints() { return m_constraints.get(); }

    void setVideoSampleBufferDelegate(AVCaptureVideoDataOutput*);
    void setAudioSampleBufferDelegate(AVCaptureAudioDataOutput*);

private:
    void setupSession();

    RetainPtr<WebCoreAVMediaCaptureSourceObserver> m_objcObserver;
    RefPtr<MediaConstraints> m_constraints;
    MediaStreamSourceStates m_currentStates;
    RetainPtr<AVCaptureSession> m_session;
    RetainPtr<AVCaptureDevice> m_device;
    
    bool m_isRunning;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // AVMediaCaptureSource_h
