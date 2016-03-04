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

#ifndef AVCaptureDeviceManager_h
#define AVCaptureDeviceManager_h

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "CaptureDeviceManager.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS AVCaptureDevice;
OBJC_CLASS AVCaptureSession;
OBJC_CLASS NSString;
OBJC_CLASS WebCoreAVCaptureDeviceManagerObserver;

namespace WebCore {

class AVCaptureSessionInfo : public CaptureSessionInfo {
public:
    AVCaptureSessionInfo(AVCaptureSession*);
    bool supportsVideoSize(const String&) const override;
    String bestSessionPresetForVideoDimensions(int width, int height) const override;

private:
    AVCaptureSession *m_platformSession;
};

class AVCaptureDeviceManager final : public CaptureDeviceManager {
    friend class NeverDestroyed<AVCaptureDeviceManager>;
public:
    Vector<CaptureDeviceInfo>& captureDeviceList() override;

    static AVCaptureDeviceManager& singleton();

    RefPtr<RealtimeMediaSource> sourceWithUID(const String&, RealtimeMediaSource::Type, MediaConstraints*) override;
    Vector<RefPtr<RealtimeMediaSource>> bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type, PassRefPtr<MediaConstraints>) override;

    TrackSourceInfoVector getSourcesInfo(const String&) override;
    bool verifyConstraintsForMediaType(RealtimeMediaSource::Type, MediaConstraints*, const CaptureSessionInfo*, String&) override;

    void deviceConnected();
    void deviceDisconnected(AVCaptureDevice*);
    
    const RealtimeMediaSourceSupportedConstraints& supportedConstraints();

protected:
    static bool isAvailable();

    AVCaptureDeviceManager();
    ~AVCaptureDeviceManager() override;
    bool sessionSupportsConstraint(const CaptureSessionInfo*, RealtimeMediaSource::Type, const String& name, const String& value) override;
    RealtimeMediaSource* createMediaSourceForCaptureDeviceWithConstraints(const CaptureDeviceInfo&, MediaConstraints*) override;
    CaptureSessionInfo defaultCaptureSession() const override;
    void refreshCaptureDeviceList() override;
    bool isSupportedFrameRate(float frameRate) const override;

    void registerForDeviceNotifications();

    RetainPtr<WebCoreAVCaptureDeviceManagerObserver> m_objcObserver;
    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
    Vector<CaptureDeviceInfo> m_devices;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // AVCaptureDeviceManager_h
