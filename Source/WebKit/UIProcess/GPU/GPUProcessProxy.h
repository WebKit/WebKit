/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "AuxiliaryProcessProxy.h"
#include "ProcessLauncher.h"
#include "ProcessThrottler.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/IntDegrees.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <memory>
#include <pal/SessionID.h>
#include <wtf/TZoneMalloc.h>

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
#include "LayerHostingContext.h"
#endif

#if PLATFORM(MAC)
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

namespace WebCore {
class CaptureDevice;
class SecurityOriginData;

struct MockMediaDevice;
struct ScreenProperties;

enum class DisplayCapturePromptType : uint8_t;
enum class VideoFrameRotation : uint16_t;
}

namespace WebKit {

enum class ProcessTerminationReason : uint8_t;

class SandboxExtensionHandle;
class WebPageProxy;
class WebProcessProxy;
class WebsiteDataStore;

struct GPUProcessConnectionParameters;
struct GPUProcessCreationParameters;
struct SharedPreferencesForWebProcess;

class GPUProcessProxy final : public AuxiliaryProcessProxy {
    WTF_MAKE_TZONE_ALLOCATED(GPUProcessProxy);
    WTF_MAKE_NONCOPYABLE(GPUProcessProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(GPUProcessProxy);
    friend LazyNeverDestroyed<GPUProcessProxy>;
public:
    static void keepProcessAliveTemporarily();
    static Ref<GPUProcessProxy> getOrCreate();
    static GPUProcessProxy* singletonIfCreated();
    ~GPUProcessProxy();

    void createGPUProcessConnection(WebProcessProxy&, IPC::Connection::Handle&&, GPUProcessConnectionParameters&&);

    void sharedPreferencesForWebProcessDidChange(WebProcessProxy&, SharedPreferencesForWebProcess&&, CompletionHandler<void()>&&);

    void updateProcessAssertion();

#if ENABLE(MEDIA_STREAM)
    void setUseMockCaptureDevices(bool);
    void setUseSCContentSharingPicker(bool);
    void enableMicrophoneMuteStatusAPI();
    void setOrientationForMediaCapture(WebCore::IntDegrees);
    void rotationAngleForCaptureDeviceChanged(const String&, WebCore::VideoFrameRotation);
    void startMonitoringCaptureDeviceRotation(WebCore::PageIdentifier, const String&);
    void stopMonitoringCaptureDeviceRotation(WebCore::PageIdentifier, const String&);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier, WebPageProxyIdentifier, CompletionHandler<void()>&&);
    void updateCaptureOrigin(const WebCore::SecurityOriginData&, WebCore::ProcessIdentifier);
    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String&);
    void setMockMediaDeviceIsEphemeral(const String&, bool);
    void resetMockMediaDevices();
    void setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted);
    void triggerMockCaptureConfigurationChange(bool forMicrophone, bool forDisplay);
    void updateSandboxAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture);
    void setShouldListenToVoiceActivity(const WebPageProxy&, bool);
    void setPageUsingMicrophone(WebPageProxyIdentifier identifier) { m_lastPageUsingMicrophone = identifier; }
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
    void promptForGetDisplayMedia(WebCore::DisplayCapturePromptType, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
    void cancelGetDisplayMediaPrompt();
#endif

    void removeSession(PAL::SessionID);

#if PLATFORM(MAC)
    void setScreenProperties(const WebCore::ScreenProperties&);
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY)
    void enablePowerLogging();
    static bool isPowerLoggingInTaskMode();
#endif
#if ENABLE(WEBXR)
    void webXRPromptAccepted(std::optional<WebCore::ProcessIdentity>, CompletionHandler<void(bool)>&&);
#endif

    void updatePreferences(WebProcessProxy&);
    void updateScreenPropertiesIfNeeded();

    void childConnectionDidBecomeUnresponsive();

    void terminateForTesting();
    void webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&&);

#if ENABLE(VIDEO)
    void requestBitmapImageForCurrentTime(WebCore::ProcessIdentifier, WebCore::MediaPlayerIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&);
#endif

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    bool hasSentGPUToolsSandboxExtensions() const { return m_hasSentGPUToolsSandboxExtensions; }
    static Vector<SandboxExtensionHandle> createGPUToolsSandboxExtensionHandlesIfNeeded();
#endif

private:
    explicit GPUProcessProxy();

    Type type() const final { return Type::GraphicsProcessing; }

    void addSession(const WebsiteDataStore&);

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "GPU"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void connectionWillOpen(IPC::Connection&) override;
    void processWillShutDown(IPC::Connection&) override;

    void gpuProcessExited(ProcessTerminationReason);

    // ProcessThrottlerClient
    ASCIILiteral clientName() const final { return "GPUProcess"_s; }
    void sendPrepareToSuspend(IsSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&&) final;
    void sendProcessDidResume(ResumeReason) final;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) override;

    // ResponsivenessTimer::Client
    void didBecomeUnresponsive() final;

    void terminateWebProcess(WebCore::ProcessIdentifier);
    void processIsReadyToExit();

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextForVisibilityPropagation(WebPageProxyIdentifier, WebCore::PageIdentifier, LayerHostingContextID);
#endif

#if ENABLE(VP9)
    void setHasVP9HardwareDecoder(bool hasVP9HardwareDecoder) { s_hasVP9HardwareDecoder = hasVP9HardwareDecoder; }
#endif
#if ENABLE(AV1)
    void setHasAV1HardwareDecoder(bool hasAV1HardwareDecoder) { s_hasAV1HardwareDecoder = hasAV1HardwareDecoder; }
#endif

#if ENABLE(MEDIA_STREAM)
    void voiceActivityDetected();
    void microphoneMuteStatusChanged(bool isMuting);
#if PLATFORM(IOS_FAMILY)
    void statusBarWasTapped(CompletionHandler<void()>&&);
#endif
#endif // ENABLE(MEDIA_STREAM)

    GPUProcessCreationParameters processCreationParameters();
    void platformInitializeGPUProcessParameters(GPUProcessCreationParameters&);

#if USE(EXTENSIONKIT)
    void sendBookmarkDataForCacheDirectory();
#endif

    RefPtr<ProcessThrottler::Activity> m_activityFromWebProcesses;
#if ENABLE(MEDIA_STREAM)
    bool m_useMockCaptureDevices { false };
    WebCore::IntDegrees m_orientation { 0 };
    WeakHashSet<WebPageProxy> m_pagesListeningToVoiceActivity;
    bool m_shouldListenToVoiceActivity { false };
    Markable<WebPageProxyIdentifier> m_lastPageUsingMicrophone;
    bool m_isMicrophoneMuteStatusAPIEnabled { false };
#endif
#if HAVE(SC_CONTENT_SHARING_PICKER)
    bool m_useSCContentSharingPicker { false };
#endif
#if PLATFORM(COCOA)
    bool m_hasSentTCCDSandboxExtension { false };
    bool m_hasSentCameraSandboxExtension { false };
    bool m_hasSentMicrophoneSandboxExtension { false };
    bool m_hasSentDisplayCaptureSandboxExtension { false };
    bool m_hasSentGPUToolsSandboxExtensions { false };
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
    bool m_hasEnabledScreenCaptureKit { false };
#endif
#if ENABLE(VP9)
    static std::optional<bool> s_hasVP9HardwareDecoder;
#endif
#if ENABLE(AV1)
    static std::optional<bool> s_hasAV1HardwareDecoder;
#endif

    HashSet<PAL::SessionID> m_sessionIDs;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
