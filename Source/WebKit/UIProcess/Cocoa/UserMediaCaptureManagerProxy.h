/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "MessageReceiver.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "UserMediaCaptureManager.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/IntDegrees.h>
#include <WebCore/OrientationNotifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceIdentifier.h>
#include <pal/spi/cocoa/TCCSPI.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebKit {
class UserMediaCaptureManagerProxy;
}

namespace WebCore {
class PlatformMediaSessionManager;
class SharedMemory;
struct VideoPresetData;
}

namespace WebKit {

class WebProcessProxy;
class UserMediaCaptureManagerProxySourceProxy;

class UserMediaCaptureManagerProxy : public IPC::MessageReceiver, public RefCounted<UserMediaCaptureManagerProxy> {
    WTF_MAKE_TZONE_ALLOCATED(UserMediaCaptureManagerProxy);
public:
    class ConnectionProxy {
    public:
        virtual ~ConnectionProxy() = default;
        virtual void addMessageReceiver(IPC::ReceiverName, IPC::MessageReceiver&) = 0;
        virtual void removeMessageReceiver(IPC::ReceiverName) = 0;
        virtual IPC::Connection& connection() = 0;
        Ref<IPC::Connection> protectedConnection() { return connection(); }
        virtual bool willStartCapture(WebCore::CaptureDevice::DeviceType) const = 0;
        virtual Logger& logger() = 0;
        Ref<Logger> protectedLogger() { return logger(); };
        virtual bool setCaptureAttributionString() { return true; }
        virtual const WebCore::ProcessIdentity& resourceOwner() const = 0;
#if ENABLE(APP_PRIVACY_REPORT)
        virtual void setTCCIdentity() { }
#endif
#if ENABLE(EXTENSION_CAPABILITIES)
        virtual bool setCurrentMediaEnvironment(WebCore::PageIdentifier) { return false; };
#endif
        virtual void startProducingData(WebCore::CaptureDevice::DeviceType) { }
        virtual RemoteVideoFrameObjectHeap* remoteVideoFrameObjectHeap() { return nullptr; }

        virtual void startMonitoringCaptureDeviceRotation(WebCore::PageIdentifier, const String&) { }
        virtual void stopMonitoringCaptureDeviceRotation(WebCore::PageIdentifier, const String&) { }
    };
    static Ref<UserMediaCaptureManagerProxy> create(UniqueRef<ConnectionProxy>&&);
    ~UserMediaCaptureManagerProxy();

    void close();
    void clear();

    void setOrientation(WebCore::IntDegrees);
    void rotationAngleForCaptureDeviceChanged(const String&, WebCore::VideoFrameRotation);

    void didReceiveMessageFromGPUProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    bool hasSourceProxies() const;

private:
    explicit UserMediaCaptureManagerProxy(UniqueRef<ConnectionProxy>&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    using CreateSourceCallback = CompletionHandler<void(const WebCore::CaptureSourceError&, const WebCore::RealtimeMediaSourceSettings&, const WebCore::RealtimeMediaSourceCapabilities&)>;
    void createMediaSourceForCaptureDeviceWithConstraints(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice& deviceID, WebCore::MediaDeviceHashSalts&&, WebCore::MediaConstraints&&, bool shouldUseGPUProcessRemoteFrames, WebCore::PageIdentifier, CreateSourceCallback&&);
    void startProducingData(WebCore::RealtimeMediaSourceIdentifier, WebCore::PageIdentifier);
    void stopProducingData(WebCore::RealtimeMediaSourceIdentifier);
    void removeSource(WebCore::RealtimeMediaSourceIdentifier);
    void capabilities(WebCore::RealtimeMediaSourceIdentifier, CompletionHandler<void(WebCore::RealtimeMediaSourceCapabilities&&)>&&);
    void applyConstraints(WebCore::RealtimeMediaSourceIdentifier, WebCore::MediaConstraints&&);
    void clone(WebCore::RealtimeMediaSourceIdentifier clonedID, WebCore::RealtimeMediaSourceIdentifier cloneID, WebCore::PageIdentifier);
    void endProducingData(WebCore::RealtimeMediaSourceIdentifier);
    void setShouldApplyRotation(WebCore::RealtimeMediaSourceIdentifier, bool shouldApplyRotation);
    void setIsInBackground(WebCore::RealtimeMediaSourceIdentifier, bool);
    void isPowerEfficient(WebCore::RealtimeMediaSourceIdentifier, CompletionHandler<void(bool)>&&);

    using TakePhotoCallback = CompletionHandler<void(Expected<std::pair<Vector<uint8_t>, String>, String>&&)>;
    void takePhoto(WebCore::RealtimeMediaSourceIdentifier, WebCore::PhotoSettings&&, TakePhotoCallback&&);

    using GetPhotoCapabilitiesCallback = CompletionHandler<void(Expected<WebCore::PhotoCapabilities, String>&&)>;
    void getPhotoCapabilities(WebCore::RealtimeMediaSourceIdentifier, GetPhotoCapabilitiesCallback&&);

    using GetPhotoSettingsCallback = CompletionHandler<void(Expected<WebCore::PhotoSettings, String>&&)>;
    void getPhotoSettings(WebCore::RealtimeMediaSourceIdentifier, GetPhotoSettingsCallback&&);

    WebCore::CaptureSourceOrError createMicrophoneSource(const WebCore::CaptureDevice&, WebCore::MediaDeviceHashSalts&&, const WebCore::MediaConstraints*, WebCore::PageIdentifier);
    WebCore::CaptureSourceOrError createCameraSource(const WebCore::CaptureDevice&, WebCore::MediaDeviceHashSalts&&, WebCore::PageIdentifier);

    using SerialAction = Function<Ref<GenericPromise>()>;
    void queueAndProcessSerialAction(SerialAction&&);

    friend class UserMediaCaptureManagerProxySourceProxy;
    HashMap<WebCore::RealtimeMediaSourceIdentifier, Ref<UserMediaCaptureManagerProxySourceProxy>> m_proxies;
    UniqueRef<ConnectionProxy> m_connectionProxy;
    WebCore::OrientationNotifier m_orientationNotifier { 0 };
    Ref<GenericPromise> m_pendingAction { GenericPromise::createAndResolve() };

    struct PageSources {
        ThreadSafeWeakPtr<WebCore::RealtimeMediaSource> microphoneSource;
        ThreadSafeWeakHashSet<WebCore::RealtimeMediaSource> cameraSources;
    };
    HashMap<WebCore::PageIdentifier, PageSources> m_pageSources;
};

}

#endif
