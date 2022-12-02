/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "ProcessTerminationReason.h"
#include "ProcessThrottler.h"
#include "ProcessThrottlerClient.h"
#include "ShareableBitmap.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <memory>
#include <pal/SessionID.h>

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
#include "LayerHostingContext.h"
#endif

#if PLATFORM(MAC)
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

namespace WebCore {
class CaptureDevice;
struct MockMediaDevice;
struct ScreenProperties;
struct SecurityOriginData;
}

namespace WebKit {

class WebProcessProxy;
class WebsiteDataStore;
struct GPUProcessConnectionParameters;
struct GPUProcessCreationParameters;

class GPUProcessProxy final : public AuxiliaryProcessProxy, private ProcessThrottlerClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(GPUProcessProxy);
    friend LazyNeverDestroyed<GPUProcessProxy>;
public:
    static Ref<GPUProcessProxy> getOrCreate();
    static GPUProcessProxy* singletonIfCreated();
    ~GPUProcessProxy();

    void createGPUProcessConnection(WebProcessProxy&, IPC::Connection::Handle&&, GPUProcessConnectionParameters&&);

    ProcessThrottler& throttler() final { return m_throttler; }
    void updateProcessAssertion();

#if ENABLE(MEDIA_STREAM)
    void setUseMockCaptureDevices(bool);
    void setOrientationForMediaCapture(uint64_t orientation);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier, CompletionHandler<void()>&&);
    void updateCaptureOrigin(const WebCore::SecurityOriginData&, WebCore::ProcessIdentifier);
    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String&);
    void setMockMediaDeviceIsEphemeral(const String&, bool);
    void resetMockMediaDevices();
    void setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted);
    void triggerMockMicrophoneConfigurationChange();
    void updateSandboxAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture);
#endif

#if HAVE(SC_CONTENT_SHARING_SESSION)
    void showWindowPicker(CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
    void showScreenPicker(CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
#endif

    void removeSession(PAL::SessionID);

#if PLATFORM(MAC)
    void displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
    void setScreenProperties(const WebCore::ScreenProperties&);
#endif

#if HAVE(POWERLOG_TASK_MODE_QUERY)
    void enablePowerLogging();
    static bool isPowerLoggingInTaskMode();
#endif

    void updatePreferences(WebProcessProxy&);
    void updateScreenPropertiesIfNeeded();

    void terminateForTesting();
    void webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&&);

    void requestBitmapImageForCurrentTime(WebCore::ProcessIdentifier, WebCore::MediaPlayerIdentifier, CompletionHandler<void(const ShareableBitmapHandle&)>&&);

private:
    explicit GPUProcessProxy();

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
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

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
#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
    void statusBarWasTapped(CompletionHandler<void()>&&);
#endif

    GPUProcessCreationParameters processCreationParameters();
    void platformInitializeGPUProcessParameters(GPUProcessCreationParameters&);

    ProcessThrottler m_throttler;
    ProcessThrottler::ActivityVariant m_activityFromWebProcesses;
#if ENABLE(MEDIA_STREAM)
    bool m_useMockCaptureDevices { false };
    uint64_t m_orientation { 0 };
#endif
#if PLATFORM(COCOA)
    bool m_hasSentTCCDSandboxExtension { false };
    bool m_hasSentCameraSandboxExtension { false };
    bool m_hasSentMicrophoneSandboxExtension { false };
    bool m_hasSentDisplayCaptureSandboxExtension { false };
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
    bool m_hasEnabledScreenCaptureKit { false };
#endif
#if ENABLE(VP9)
    static std::optional<bool> s_hasVP9HardwareDecoder;
#endif

    HashSet<PAL::SessionID> m_sessionIDs;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
