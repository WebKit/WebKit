/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "UserMediaPermissionCheckProxy.h"
#include "UserMediaPermissionRequestProxy.h"
#include <WebCore/MediaProducer.h>
#include <WebCore/PermissionDescriptor.h>
#include <WebCore/PermissionState.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSourceFactory.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class UserMediaPermissionRequestManagerProxy;
}

namespace WebCore {
class CaptureDevice;
class SecurityOrigin;

enum class MediaConstraintType : uint8_t;
enum class PermissionName : uint8_t;

struct CaptureDeviceWithCapabilities;
struct ClientOrigin;
struct MediaConstraints;
struct MediaStreamRequest;
};

OBJC_CLASS AVCaptureDeviceRotationCoordinator;
OBJC_CLASS WKRotationCoordinatorObserver;

namespace WebKit {

class WebFrameProxy;
class WebPageProxy;

enum class MediaDevicePermissionRequestIdentifierType { };
using MediaDevicePermissionRequestIdentifier = ObjectIdentifier<MediaDevicePermissionRequestIdentifierType>;

class UserMediaPermissionRequestManagerProxy final
    : public RefCountedAndCanMakeWeakPtr<UserMediaPermissionRequestManagerProxy>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(UserMediaPermissionRequestManagerProxy);
public:
    static Ref<UserMediaPermissionRequestManagerProxy> create(WebPageProxy&);
    ~UserMediaPermissionRequestManagerProxy();

    WebPageProxy* page() const;
    RefPtr<WebPageProxy> protectedPage() const;

    void disconnectFromPage();

#if ENABLE(MEDIA_STREAM)
    static void forEach(const WTF::Function<void(UserMediaPermissionRequestManagerProxy&)>&);
#if HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
    bool isMonitoringCaptureDeviceRotation(const String&);
    void startMonitoringCaptureDeviceRotation(const String&);
    void stopMonitoringCaptureDeviceRotation(const String&);
    void rotationAngleForCaptureDeviceChanged(const String&, WebCore::VideoFrameRotation);
#endif // HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
#endif
    static bool permittedToCaptureAudio();
    static bool permittedToCaptureVideo();

    void invalidatePendingRequests();

    void requestUserMediaPermissionForFrame(WebCore::UserMediaRequestIdentifier, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&&  userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, WebCore::MediaStreamRequest&&);

    void resetAccess(WebFrameProxy* = nullptr);
    void didCommitLoadForFrame(WebCore::FrameIdentifier);
    void viewIsBecomingVisible();

    void grantRequest(UserMediaPermissionRequestProxy&);
    void denyRequest(UserMediaPermissionRequestProxy&, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason, const String& message = { });
    void denyRequest(UserMediaPermissionRequestProxy&, WebCore::MediaConstraintType);

    void enumerateMediaDevicesForFrame(WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(const Vector<WebCore::CaptureDeviceWithCapabilities>&, WebCore::MediaDeviceHashSalts&&)>&&);

    void stopCapture();
    void scheduleNextRejection();
    void rejectionTimerFired();
    void clearCachedState();
    void captureDevicesChanged();

    void captureStateChanged(WebCore::MediaProducerMediaStateFlags oldState, WebCore::MediaProducerMediaStateFlags newState);
    void syncWithWebCorePrefs() const;

    enum class RequestAction {
        Deny,
        Grant,
        Prompt
    };

    bool canAudioCaptureSucceed() const;
    bool canVideoCaptureSucceed() const;
    void setMockCaptureDevicesEnabledOverride(std::optional<bool>);
    bool hasPendingCapture() const { return m_hasPendingCapture; }

    void checkUserMediaPermissionForSpeechRecognition(WebCore::FrameIdentifier, const WebCore::SecurityOrigin&, const WebCore::SecurityOrigin&, const WebCore::CaptureDevice&, CompletionHandler<void(bool)>&&);

    struct DeniedRequest {
        WebCore::FrameIdentifier mainFrameID;
        Ref<WebCore::SecurityOrigin> userMediaDocumentOrigin;
        Ref<WebCore::SecurityOrigin> topLevelDocumentOrigin;
        bool isAudioDenied;
        bool isVideoDenied;
        bool isScreenCaptureDenied;
    };

    std::optional<WebCore::PermissionState> filterPermissionQuery(const WebCore::ClientOrigin&, const WebCore::PermissionDescriptor&, WebCore::PermissionState);

    bool shouldChangeDeniedToPromptForCamera(const WebCore::ClientOrigin&) const;
    bool shouldChangeDeniedToPromptForMicrophone(const WebCore::ClientOrigin&) const;
    bool shouldChangePromptToGrantForCamera(const WebCore::ClientOrigin&) const;
    bool shouldChangePromptToGrantForMicrophone(const WebCore::ClientOrigin&) const;

    void clearUserMediaPermissionRequestHistory(WebCore::PermissionName);

private:
    explicit UserMediaPermissionRequestManagerProxy(WebPageProxy&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const override { return "UserMediaPermissionRequestManagerProxy"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    void denyRequest(UserMediaPermissionRequestProxy&, UserMediaPermissionRequestProxy::UserMediaAccessDenialReason, const String& message, WebCore::MediaConstraintType);

    Ref<UserMediaPermissionRequestProxy> createPermissionRequest(WebCore::UserMediaRequestIdentifier, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&&);
#if ENABLE(MEDIA_STREAM)
    void finishGrantingRequest(UserMediaPermissionRequestProxy&);

    const UserMediaPermissionRequestProxy* searchForGrantedRequest(std::optional<WebCore::FrameIdentifier>, const WebCore::SecurityOrigin& userMediaDocumentOrigin, const WebCore::SecurityOrigin& topLevelDocumentOrigin, bool needsAudio, bool needsVideo) const;
    bool wasRequestDenied(const UserMediaPermissionRequestProxy&, bool needsAudio, bool needsVideo, bool needsScreenCapture);

    using PermissionInfo = UserMediaPermissionCheckProxy::PermissionInfo;
    void getUserMediaPermissionInfo(WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, CompletionHandler<void(PermissionInfo)>&&);
    void captureDevicesChanged(PermissionInfo);

    RequestAction getRequestAction(const UserMediaPermissionRequestProxy&);

    bool wasGrantedVideoOrAudioAccess(WebCore::FrameIdentifier);
    bool wasGrantedAudioAccess(WebCore::FrameIdentifier);
    bool wasGrantedVideoAccess(WebCore::FrameIdentifier);

    void computeFilteredDeviceList(WebCore::FrameIdentifier, bool revealIdsAndLabels, CompletionHandler<void(Vector<WebCore::CaptureDeviceWithCapabilities>&&)>&&);
    void platformGetMediaStreamDevices(bool revealIdsAndLabels, CompletionHandler<void(Vector<WebCore::CaptureDeviceWithCapabilities>&&)>&&);

    void processUserMediaPermissionRequest();
    void processUserMediaPermissionInvalidRequest(WebCore::MediaConstraintType invalidConstraint);
    void processUserMediaPermissionValidRequest(Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaDeviceHashSalts&&);
    void startProcessingUserMediaPermissionRequest(Ref<UserMediaPermissionRequestProxy>&&);

    static void requestSystemValidation(const WebPageProxy&, UserMediaPermissionRequestProxy&, CompletionHandler<void(bool)>&&);

    void platformValidateUserMediaRequestConstraints(WebCore::RealtimeMediaSourceCenter::ValidConstraintsHandler&& validHandler, WebCore::RealtimeMediaSourceCenter::InvalidConstraintsHandler&& invalidHandler, WebCore::MediaDeviceHashSalts&&);
#endif

    bool mockCaptureDevicesEnabled() const;

    void watchdogTimerFired();

    void processNextUserMediaRequestIfNeeded();
    void decidePolicyForUserMediaPermissionRequest();
    void updateStoredRequests(UserMediaPermissionRequestProxy&);

    String ephemeralDeviceHashSaltForFrame(WebCore::FrameIdentifier);

    RefPtr<UserMediaPermissionRequestProxy> m_currentUserMediaRequest;
    Deque<Ref<UserMediaPermissionRequestProxy>> m_pendingUserMediaRequests;
    HashSet<MediaDevicePermissionRequestIdentifier> m_pendingDeviceRequests;

    WeakPtr<WebPageProxy> m_page;

    RunLoop::Timer m_rejectionTimer;
    Deque<Ref<UserMediaPermissionRequestProxy>> m_pendingRejections;

    Vector<Ref<UserMediaPermissionRequestProxy>> m_pregrantedRequests;
    Vector<Ref<UserMediaPermissionRequestProxy>> m_grantedRequests;
    HashMap<WebCore::FrameIdentifier, String> m_frameEphemeralHashSalts;

    Vector<DeniedRequest> m_deniedRequests;

    WebCore::MediaProducerMediaStateFlags m_captureState;
    RunLoop::Timer m_watchdogTimer;
    Seconds m_currentWatchdogInterval;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
#if PLATFORM(COCOA)
    bool m_hasCreatedSandboxExtensionForTCCD { false };
#endif
    uint64_t m_hasPendingCapture { 0 };
    std::optional<bool> m_mockDevicesEnabledOverride;
    HashSet<WebCore::FrameIdentifier> m_grantedAudioFrames;
    HashSet<WebCore::FrameIdentifier> m_grantedVideoFrames;
#if PLATFORM(COCOA)
    HashCountedSet<String> m_monitoredDeviceIds;
    RetainPtr<WKRotationCoordinatorObserver> m_objcObserver;
#endif
};

String convertEnumerationToString(UserMediaPermissionRequestManagerProxy::RequestAction);

} // namespace WebKit

#if ENABLE(MEDIA_STREAM)
namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebKit::UserMediaPermissionRequestManagerProxy::RequestAction> {
    static String toString(const WebKit::UserMediaPermissionRequestManagerProxy::RequestAction type)
    {
        return convertEnumerationToString(type);
    }
};

}; // namespace WTF
#endif
