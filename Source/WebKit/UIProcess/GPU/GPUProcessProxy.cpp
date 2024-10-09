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

#include "config.h"
#include "GPUProcessProxy.h"

#if ENABLE(GPU_PROCESS)

#include "DrawingAreaProxy.h"
#include "GPUProcessConnectionParameters.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessMessages.h"
#include "GPUProcessPreferences.h"
#include "GPUProcessProxyMessages.h"
#include "GPUProcessSessionParameters.h"
#include "Logging.h"
#include "OverrideLanguages.h"
#include "ProvisionalPageProxy.h"
#include "WebPageGroup.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/DisplayCapturePromptType.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/ScreenProperties.h>
#include <wtf/CompletionHandler.h>
#include <wtf/LogInitialization.h>
#include <wtf/MachSendRight.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TranslatedProcess.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/AGXCompilerService.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
#include "SandboxUtilities.h"
#include <wtf/FileSystem.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/BlockPtr.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "DRMDevice.h"
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
#include "ExtensionCapabilityGrant.h"
#include "MediaCapability.h"
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(MEDIA_STREAM) && HAVE(AUDIT_TOKEN)
static bool shouldCreateAppleCameraServiceSandboxExtension()
{
#if !PLATFORM(MAC) && !PLATFORM(MACCATALYST)
    return false;
#elif CPU(ARM64)
    return true;
#else
    return WTF::isX86BinaryRunningOnARM();
#endif
}
#endif

static WeakPtr<GPUProcessProxy>& singleton()
{
    static NeverDestroyed<WeakPtr<GPUProcessProxy>> singleton;
    return singleton;
}

static RefPtr<GPUProcessProxy>& keptAliveGPUProcessProxy()
{
    static MainThreadNeverDestroyed<RefPtr<GPUProcessProxy>> keptAliveGPUProcessProxy;
    return keptAliveGPUProcessProxy.get();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUProcessProxy);

void GPUProcessProxy::keepProcessAliveTemporarily()
{
    ASSERT(isMainRunLoop());
    static constexpr auto durationToKeepGPUProcessAliveAfterDestruction = 1_min;

    if (!singleton())
        return;

    keptAliveGPUProcessProxy() = singleton().get();
    static NeverDestroyed<RunLoop::Timer> releaseGPUProcessTimer(RunLoop::main(), [] {
        keptAliveGPUProcessProxy() = nullptr;
    });
    releaseGPUProcessTimer.get().startOneShot(durationToKeepGPUProcessAliveAfterDestruction);
}

Ref<GPUProcessProxy> GPUProcessProxy::getOrCreate()
{
    ASSERT(RunLoop::isMain());
    if (auto& existingGPUProcess = singleton()) {
        ASSERT(existingGPUProcess->state() != State::Terminated);
        return *existingGPUProcess;
    }
    Ref gpuProcess = adoptRef(*new GPUProcessProxy);
    singleton() = gpuProcess;
    return gpuProcess;
}

GPUProcessProxy* GPUProcessProxy::singletonIfCreated()
{
    return singleton().get();
}

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
static String gpuProcessCachesDirectory()
{
    constexpr ASCIILiteral cacheDirectory = "/Library/Caches/com.apple.WebKit.GPU/"_s;

    String path = WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory(cacheDirectory);

    FileSystem::makeAllDirectories(path);
    
    return path;
}
#endif

GPUProcessProxy::GPUProcessProxy()
    : AuxiliaryProcessProxy(WebProcessPool::anyProcessPoolNeedsUIBackgroundAssertion() ? ShouldTakeUIBackgroundAssertion::Yes : ShouldTakeUIBackgroundAssertion::No)
#if ENABLE(MEDIA_STREAM)
    , m_useMockCaptureDevices(MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled())
#endif
{
    connect();

    GPUProcessCreationParameters parameters;
    parameters.auxiliaryProcessParameters = auxiliaryProcessParameters();
    parameters.overrideLanguages = overrideLanguages();

#if ENABLE(MEDIA_STREAM)
    parameters.useMockCaptureDevices = m_useMockCaptureDevices;
#if PLATFORM(MAC)
    // FIXME: Remove this and related parameter when <rdar://problem/29448368> is fixed.
    if (WTF::MacApplication::isSafari()) {
        if (auto handle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone"_s))
            parameters.microphoneSandboxExtensionHandle = WTFMove(*handle);
        m_hasSentMicrophoneSandboxExtension = true;
    }
#endif
#endif // ENABLE(MEDIA_STREAM)

    parameters.parentPID = getCurrentProcessID();

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    parameters.containerCachesDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(gpuProcessCachesDirectory());
    auto containerTemporaryDirectory = WebsiteDataStore::defaultResolvedContainerTemporaryDirectory();

    if (!parameters.containerCachesDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.containerCachesDirectory, SandboxExtension::Type::ReadWrite))
            parameters.containerCachesDirectoryExtensionHandle = WTFMove(*handle);
    }

    if (!containerTemporaryDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(containerTemporaryDirectory, SandboxExtension::Type::ReadWrite))
            parameters.containerTemporaryDirectoryExtensionHandle = WTFMove(*handle);
    }
#endif
#if PLATFORM(IOS_FAMILY) && HAVE(AGX_COMPILER_SERVICE)
    if (WebCore::deviceHasAGXCompilerService()) {
        parameters.compilerServiceExtensionHandles = SandboxExtension::createHandlesForMachLookup(WebCore::agxCompilerServices(), std::nullopt);
        parameters.dynamicIOKitExtensionHandles = SandboxExtension::createHandlesForIOKitClassExtensions(WebCore::agxCompilerClasses(), std::nullopt);
    }
#endif

#if USE(GBM)
    parameters.renderDeviceFile = drmRenderNodeDevice();
#endif

    platformInitializeGPUProcessParameters(parameters);

#if PLATFORM(COCOA)
    m_hasSentGPUToolsSandboxExtensions = !parameters.gpuToolsExtensionHandles.isEmpty();
#endif

    // Initialize the GPU process.
    sendWithAsyncReply(Messages::GPUProcess::InitializeGPUProcess(WTFMove(parameters)), [initializationActivityAndGrant = initializationActivityAndGrant()] { });

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS) && ENABLE(AUDIO_COMPONENT_SERVER_REGISTRATIONS_IN_GPU_PROCESS)
    auto registrations = fetchAudioComponentServerRegistrations();
    RELEASE_ASSERT(registrations);
    send(Messages::GPUProcess::ConsumeAudioComponentRegistrations(IPC::SharedBufferReference(WTFMove(registrations))), 0);
#endif

    updateProcessAssertion();
}

GPUProcessProxy::~GPUProcessProxy() = default;

#if ENABLE(MEDIA_STREAM)
void GPUProcessProxy::setUseMockCaptureDevices(bool value)
{
    if (value == m_useMockCaptureDevices)
        return;
    m_useMockCaptureDevices = value;
    send(Messages::GPUProcess::SetMockCaptureDevicesEnabled { m_useMockCaptureDevices }, 0);
}

void GPUProcessProxy::setUseSCContentSharingPicker(bool use)
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    if (use == m_useSCContentSharingPicker)
        return;
    m_useSCContentSharingPicker = use;
    send(Messages::GPUProcess::SetUseSCContentSharingPicker { m_useSCContentSharingPicker }, 0);
#else
    UNUSED_PARAM(use);
#endif
}

void GPUProcessProxy::enableMicrophoneMuteStatusAPI()
{
    if (m_isMicrophoneMuteStatusAPIEnabled)
        return;
    m_isMicrophoneMuteStatusAPIEnabled = true;
    send(Messages::GPUProcess::EnableMicrophoneMuteStatusAPI { }, 0);
}

void GPUProcessProxy::setOrientationForMediaCapture(WebCore::IntDegrees orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    send(Messages::GPUProcess::SetOrientationForMediaCapture { orientation }, 0);
}

#if HAVE(APPLE_CAMERA_USER_CLIENT)
static const ASCIILiteral appleCameraUserClientPath { "com.apple.aneuserd"_s };
static const ASCIILiteral appleCameraUserClientIOKitClientClass { "H11ANEInDirectPathClient"_s };
static const ASCIILiteral appleCameraUserClientIOKitServiceClass { "H11ANEIn"_s };
#endif

static inline bool addCameraSandboxExtensions(Vector<SandboxExtension::Handle>& extensions)
{
    auto sandboxExtensionHandle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.camera"_s);
    if (!sandboxExtensionHandle) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to create com.apple.webkit.camera sandbox extension");
        return false;
    }
#if HAVE(AUDIT_TOKEN)
        if (shouldCreateAppleCameraServiceSandboxExtension()) {
            auto appleCameraServicePathSandboxExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.applecamerad"_s, std::nullopt);
            if (!appleCameraServicePathSandboxExtensionHandle) {
                RELEASE_LOG_ERROR(WebRTC, "Unable to create com.apple.applecamerad sandbox extension");
                return false;
            }
            extensions.append(WTFMove(*appleCameraServicePathSandboxExtensionHandle));
#if HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
            auto additionalAppleCameraServicePathSandboxExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.appleh13camerad"_s, std::nullopt);
            if (!additionalAppleCameraServicePathSandboxExtensionHandle) {
                RELEASE_LOG_ERROR(WebRTC, "Unable to create com.apple.appleh13camerad sandbox extension");
                return false;
            }
            extensions.append(WTFMove(*additionalAppleCameraServicePathSandboxExtensionHandle));
#endif
#if HAVE(APPLE_CAMERA_USER_CLIENT)
            // Needed for rdar://108282689:
            auto appleCameraUserClientExtensionHandle = SandboxExtension::createHandleForMachLookup(appleCameraUserClientPath, std::nullopt);
            if (!appleCameraUserClientExtensionHandle) {
                RELEASE_LOG_ERROR(WebRTC, "Unable to create %s sandbox extension", appleCameraUserClientPath.characters());
                return false;
            }
            extensions.append(WTFMove(*appleCameraUserClientExtensionHandle));

            auto appleCameraUserClientIOKitClientClassExtensionHandle = SandboxExtension::createHandleForIOKitClassExtension(appleCameraUserClientIOKitClientClass, std::nullopt);
            if (!appleCameraUserClientIOKitClientClassExtensionHandle) {
                RELEASE_LOG_ERROR(WebRTC, "Unable to create %s sandbox extension", appleCameraUserClientIOKitClientClass.characters());
                return false;
            }
            extensions.append(WTFMove(*appleCameraUserClientIOKitClientClassExtensionHandle));

            auto appleCameraUserClientIOKitServiceClassExtensionHandle = SandboxExtension::createHandleForIOKitClassExtension(appleCameraUserClientIOKitServiceClass, std::nullopt);
            if (!appleCameraUserClientIOKitServiceClassExtensionHandle) {
                RELEASE_LOG_ERROR(WebRTC, "Unable to create %s sandbox extension", appleCameraUserClientIOKitServiceClass.characters());
                return false;
            }
            extensions.append(WTFMove(*appleCameraUserClientIOKitServiceClassExtensionHandle));

#endif
        }
#endif // HAVE(AUDIT_TOKEN)

    extensions.append(WTFMove(*sandboxExtensionHandle));
    return true;
}

static inline bool addMicrophoneSandboxExtension(Vector<SandboxExtension::Handle>& extensions)
{
    auto sandboxExtensionHandle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone"_s);
    if (!sandboxExtensionHandle) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to create com.apple.webkit.microphone sandbox extension");
        return false;
    }
    extensions.append(WTFMove(*sandboxExtensionHandle));
    return true;
}

#if HAVE(SCREEN_CAPTURE_KIT)
static inline bool addDisplayCaptureSandboxExtension(std::optional<audit_token_t> auditToken, Vector<SandboxExtension::Handle>& extensions)
{
    if (!auditToken) {
        RELEASE_LOG_ERROR(WebRTC, "NULL audit token");
        return false;
    }

    auto handle = SandboxExtension::createHandleForMachLookup("com.apple.replaykit.sharingsession.notification"_s, *auditToken);
    if (!handle)
        return false;
    extensions.append(WTFMove(*handle));

    handle = SandboxExtension::createHandleForMachLookup("com.apple.replaykit.sharingsession"_s, *auditToken);
    if (!handle)
        return false;
    extensions.append(WTFMove(*handle));

    handle = SandboxExtension::createHandleForMachLookup("com.apple.tccd.system"_s, *auditToken);
    if (!handle)
        return false;
    extensions.append(WTFMove(*handle));

    handle = SandboxExtension::createHandleForMachLookup("com.apple.replayd"_s, *auditToken);
    if (!handle)
        return false;
    extensions.append(WTFMove(*handle));

    return true;
}
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
static inline bool addTCCDSandboxExtension(Vector<SandboxExtension::Handle>& extensions)
{
    auto handle = SandboxExtension::createHandleForMachLookup("com.apple.tccd"_s, std::nullopt);
    if (!handle) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to create com.apple.tccd sandbox extension");
        return false;
    }
    extensions.append(WTFMove(*handle));
    return true;
}
#endif

void GPUProcessProxy::updateSandboxAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture)
{
    if (m_useMockCaptureDevices)
        return;

#if PLATFORM(COCOA)
    Vector<SandboxExtension::Handle> extensions;

    if (allowVideoCapture && !m_hasSentCameraSandboxExtension && addCameraSandboxExtensions(extensions))
        m_hasSentCameraSandboxExtension = true;

    if (allowAudioCapture && !m_hasSentMicrophoneSandboxExtension && addMicrophoneSandboxExtension(extensions))
        m_hasSentMicrophoneSandboxExtension = true;

#if HAVE(SCREEN_CAPTURE_KIT)
    if (allowDisplayCapture && !m_hasSentDisplayCaptureSandboxExtension && addDisplayCaptureSandboxExtension(protectedConnection()->getAuditToken(), extensions))
        m_hasSentDisplayCaptureSandboxExtension = true;
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
    if ((allowAudioCapture || allowVideoCapture) && !m_hasSentTCCDSandboxExtension && addTCCDSandboxExtension(extensions))
        m_hasSentTCCDSandboxExtension = true;
#endif // PLATFORM(IOS) || PLATFORM(VISION)

    if (!extensions.isEmpty())
        send(Messages::GPUProcess::UpdateSandboxAccess { WTFMove(extensions) }, 0);
#endif // PLATFORM(COCOA)
}

void GPUProcessProxy::updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier processID, WebPageProxyIdentifier pageIdentifier, CompletionHandler<void()>&& completionHandler)
{
    if (allowAudioCapture)
        m_lastPageUsingMicrophone = pageIdentifier;

    updateSandboxAccess(allowAudioCapture, allowVideoCapture, allowDisplayCapture);
    sendWithAsyncReply(Messages::GPUProcess::UpdateCaptureAccess { allowAudioCapture, allowVideoCapture, allowDisplayCapture, processID }, WTFMove(completionHandler));
}

void GPUProcessProxy::updateCaptureOrigin(const WebCore::SecurityOriginData& originData, WebCore::ProcessIdentifier processID)
{
    send(Messages::GPUProcess::UpdateCaptureOrigin { originData, processID }, 0);
}

void GPUProcessProxy::addMockMediaDevice(const WebCore::MockMediaDevice& device)
{
    send(Messages::GPUProcess::AddMockMediaDevice { device }, 0);
}

void GPUProcessProxy::clearMockMediaDevices()
{
    send(Messages::GPUProcess::ClearMockMediaDevices { }, 0);
}

void GPUProcessProxy::removeMockMediaDevice(const String& persistentId)
{
    send(Messages::GPUProcess::RemoveMockMediaDevice { persistentId }, 0);
}

void GPUProcessProxy::setMockMediaDeviceIsEphemeral(const String& persistentId, bool isEphemeral)
{
    send(Messages::GPUProcess::SetMockMediaDeviceIsEphemeral { persistentId, isEphemeral }, 0);
}

void GPUProcessProxy::resetMockMediaDevices()
{
    send(Messages::GPUProcess::ResetMockMediaDevices { }, 0);
}

void GPUProcessProxy::setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted)
{
    send(Messages::GPUProcess::SetMockCaptureDevicesInterrupted { isCameraInterrupted, isMicrophoneInterrupted }, 0);
}

void GPUProcessProxy::triggerMockCaptureConfigurationChange(bool forMicrophone, bool forDisplay)
{
    send(Messages::GPUProcess::TriggerMockCaptureConfigurationChange { forMicrophone, forDisplay }, 0);
}

void GPUProcessProxy::setShouldListenToVoiceActivity(const WebPageProxy& proxy, bool shouldListen)
{
    if (shouldListen) {
        m_pagesListeningToVoiceActivity.add(proxy);
        if (m_shouldListenToVoiceActivity)
            return;
    } else {
        m_pagesListeningToVoiceActivity.remove(proxy);
        if (!m_shouldListenToVoiceActivity)
            return;
    }

    bool shouldListenToVoiceActivity = anyOf(m_pagesListeningToVoiceActivity, [] (auto& page) {
        return page.shouldListenToVoiceActivity();
    });
    if (m_shouldListenToVoiceActivity == shouldListenToVoiceActivity)
        return;

    m_shouldListenToVoiceActivity = shouldListenToVoiceActivity;
    send(Messages::GPUProcess::SetShouldListenToVoiceActivity { shouldListenToVoiceActivity }, 0);
}
#endif // ENABLE(MEDIA_STREAM)

#if HAVE(SCREEN_CAPTURE_KIT)
void GPUProcessProxy::promptForGetDisplayMedia(WebCore::DisplayCapturePromptType type, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::PromptForGetDisplayMedia { type }, WTFMove(completionHandler));
}

void GPUProcessProxy::cancelGetDisplayMediaPrompt()
{
    send(Messages::GPUProcess::CancelGetDisplayMediaPrompt { }, 0);
}
#endif

void GPUProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::GPU;
    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);
}

void GPUProcessProxy::connectionWillOpen(IPC::Connection&)
{
    RELEASE_LOG(Process, "%p - GPUProcessProxy::connectionWillOpen:", this);
}

void GPUProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    RELEASE_LOG(Process, "%p - GPUProcessProxy::processWillShutDown:", this);
    ASSERT_UNUSED(connection, &this->connection() == &connection);
    if (singleton() == this)
        singleton() = nullptr;
}

#if ENABLE(VP9)
std::optional<bool> GPUProcessProxy::s_hasVP9HardwareDecoder;
#endif
#if ENABLE(AV1)
std::optional<bool> GPUProcessProxy::s_hasAV1HardwareDecoder;
#endif

void GPUProcessProxy::createGPUProcessConnection(WebProcessProxy& webProcessProxy, IPC::Connection::Handle&& connectionIdentifier, GPUProcessConnectionParameters&& parameters)
{
#if ENABLE(VP9)
    parameters.hasVP9HardwareDecoder = s_hasVP9HardwareDecoder;
#endif
#if ENABLE(AV1)
    parameters.hasAV1HardwareDecoder = s_hasAV1HardwareDecoder;
#endif

    if (RefPtr store = webProcessProxy.websiteDataStore())
        addSession(*store);

    RELEASE_LOG(ProcessSuspension, "%p - GPUProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    startResponsivenessTimer(UseLazyStop::No);
    sendWithAsyncReply(Messages::GPUProcess::CreateGPUConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID(), WTFMove(connectionIdentifier), WTFMove(parameters) }, [this, weakThis = WeakPtr { *this }]() mutable {
        if (!weakThis)
            return;
        stopResponsivenessTimer();
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void GPUProcessProxy::sharedPreferencesForWebProcessDidChange(WebProcessProxy& webProcessProxy, SharedPreferencesForWebProcess&& sharedPreferencesForWebProcess, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::SharedPreferencesForWebProcessDidChange { webProcessProxy.coreProcessIdentifier(), WTFMove(sharedPreferencesForWebProcess) }, WTFMove(completionHandler));
}

void GPUProcessProxy::gpuProcessExited(ProcessTerminationReason reason)
{
    Ref protectedThis { *this };

    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
    case ProcessTerminationReason::ExceededCPULimit:
    case ProcessTerminationReason::RequestedByClient:
    case ProcessTerminationReason::IdleExit:
    case ProcessTerminationReason::Unresponsive:
    case ProcessTerminationReason::Crash:
        RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::gpuProcessExited: reason=%" PUBLIC_LOG_STRING, this, processTerminationReasonToString(reason).characters());
        break;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
    case ProcessTerminationReason::RequestedByModelProcess:
    case ProcessTerminationReason::GPUProcessCrashedTooManyTimes:
    case ProcessTerminationReason::ModelProcessCrashedTooManyTimes:
        ASSERT_NOT_REACHED();
        break;
    }

#if ENABLE(EXTENSION_CAPABILITIES)
    // FIXME: Any ExtensionCapabilityGranter can invalidate the GPUProcessProxy grants, so we pick the first one. In the future ExtensionCapabilityGranter should be made a singleton.
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        processPool->extensionCapabilityGranter().invalidateGrants(moveToVector(std::exchange(extensionCapabilityGrants(), { }).values()));
        break;
    }

#endif

    if (keptAliveGPUProcessProxy() == this)
        keptAliveGPUProcessProxy() = nullptr;
    if (singleton() == this)
        singleton() = nullptr;

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->gpuProcessExited(processID(), reason);
}

void GPUProcessProxy::processIsReadyToExit()
{
    RELEASE_LOG(Process, "%p - GPUProcessProxy::processIsReadyToExit:", this);
    terminate();
    gpuProcessExited(ProcessTerminationReason::IdleExit); // May cause |this| to get deleted.
}

void GPUProcessProxy::childConnectionDidBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::childConnectionDidBecomeUnresponsive:", this);
    didBecomeUnresponsive();
}

void GPUProcessProxy::terminateForTesting()
{
    processIsReadyToExit();
}

void GPUProcessProxy::webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::WebProcessConnectionCountForTesting(), WTFMove(completionHandler));
}

void GPUProcessProxy::didClose(IPC::Connection&)
{
    RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::didClose:", this);
    gpuProcessExited(ProcessTerminationReason::Crash); // May cause |this| to get deleted.
}

void GPUProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName, int32_t)
{
    logInvalidMessage(connection, messageName);

    WebProcessPool::didReceiveInvalidMessage(messageName);

    // Terminate the GPU process.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void GPUProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    RELEASE_LOG(Process, "%p - GPUProcessProxy::didFinishLaunching:", this);

    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!connectionIdentifier) {
        gpuProcessExited(ProcessTerminationReason::Crash);
        return;
    }
    
#if PLATFORM(COCOA)
    if (auto networkProcess = NetworkProcessProxy::defaultNetworkProcess())
        networkProcess->sendXPCEndpointToProcess(*this);
#endif

    beginResponsivenessChecks();

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->gpuProcessDidFinishLaunching(processID());

#if HAVE(POWERLOG_TASK_MODE_QUERY)
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([weakThis = WeakPtr { *this }] () mutable {
        if (!isPowerLoggingInTaskMode())
            return;
        RunLoop::protectedMain()->dispatch([weakThis = WTFMove(weakThis)] () {
            if (!weakThis)
                return;
            weakThis->enablePowerLogging();
        });
    }).get());
#endif

#if USE(EXTENSIONKIT)
    sendBookmarkDataForCacheDirectory();
#endif
}

#if ENABLE(WEBXR)
void GPUProcessProxy::webXRPromptAccepted(std::optional<WebCore::ProcessIdentity> processIdentity, CompletionHandler<void(bool)>&& completion)
{
    sendWithAsyncReply(Messages::GPUProcess::WebXRPromptAccepted(WTFMove(processIdentity)), WTFMove(completion));
}
#endif


void GPUProcessProxy::updateProcessAssertion()
{
    bool hasAnyForegroundWebProcesses = false;
    bool hasAnyBackgroundWebProcesses = false;

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        hasAnyForegroundWebProcesses |= processPool->hasForegroundWebProcesses();
        hasAnyBackgroundWebProcesses |= processPool->hasBackgroundWebProcesses();
    }

    if (hasAnyForegroundWebProcesses) {
        if (!ProcessThrottler::isValidForegroundActivity(m_activityFromWebProcesses.get())) {
            m_activityFromWebProcesses = protectedThrottler()->foregroundActivity("GPU for foreground view(s)"_s);
        }
        return;
    }
    if (hasAnyBackgroundWebProcesses) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_activityFromWebProcesses.get())) {
            m_activityFromWebProcesses = protectedThrottler()->backgroundActivity("GPU for background view(s)"_s);
        }
        return;
    }

    // Use std::exchange() instead of a simple nullptr assignment to avoid re-entering this
    // function during the destructor of the ProcessThrottler activity, before setting
    // m_activityFromWebProcesses.
    std::exchange(m_activityFromWebProcesses, nullptr);
}

static inline GPUProcessSessionParameters gpuProcessSessionParameters(const WebsiteDataStore& store)
{
    GPUProcessSessionParameters parameters;

    auto& resolvedDirectories = const_cast<WebsiteDataStore&>(store).resolvedDirectories();
    parameters.mediaCacheDirectory = resolvedDirectories.mediaCacheDirectory;
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    if (!parameters.mediaCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaCacheDirectory, SandboxExtension::Type::ReadWrite))
            parameters.mediaCacheDirectorySandboxExtensionHandle = WTFMove(*handle);
    }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    parameters.mediaKeysStorageDirectory = resolvedDirectories.mediaKeysStorageDirectory;
    SandboxExtension::Handle mediaKeysStorageDirectorySandboxExtensionHandle;
    if (!parameters.mediaKeysStorageDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaKeysStorageDirectory, SandboxExtension::Type::ReadWrite))
            parameters.mediaKeysStorageDirectorySandboxExtensionHandle = WTFMove(*handle);
    }
#endif

    return parameters;
}

void GPUProcessProxy::addSession(const WebsiteDataStore& store)
{
    if (!canSendMessage())
        return;

    if (m_sessionIDs.contains(store.sessionID()))
        return;

    send(Messages::GPUProcess::AddSession { store.sessionID(), gpuProcessSessionParameters(store) }, 0);
    m_sessionIDs.add(store.sessionID());
}

void GPUProcessProxy::removeSession(PAL::SessionID sessionID)
{
    if (!canSendMessage())
        return;

    if (m_sessionIDs.remove(sessionID))
        send(Messages::GPUProcess::RemoveSession { sessionID }, 0);
}

void GPUProcessProxy::sendPrepareToSuspend(IsSuspensionImminent isSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcessProxy::sendPrepareToSuspend:", this);
    sendWithAsyncReply(Messages::GPUProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes, MonotonicTime::now() + Seconds(remainingRunTime)), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void GPUProcessProxy::sendProcessDidResume(ResumeReason)
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcessProxy::sendProcessDidResume:", this);
    if (canSendMessage())
        send(Messages::GPUProcess::ProcessDidResume(), 0);
}

void GPUProcessProxy::terminateWebProcess(WebCore::ProcessIdentifier webProcessIdentifier)
{
    RELEASE_LOG_ERROR(Process, "GPUProcessProxy::terminateWebProcess: webProcessIdentifier=%" PRIu64, webProcessIdentifier.toUInt64());
    if (auto process = WebProcessProxy::processForIdentifier(webProcessIdentifier))
        process->requestTermination(ProcessTerminationReason::RequestedByGPUProcess);
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void GPUProcessProxy::didCreateContextForVisibilityPropagation(WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier pageID, LayerHostingContextID contextID)
{
    RELEASE_LOG(Process, "%p - GPUProcessProxy::didCreateContextForVisibilityPropagation: webPageProxyID: %" PRIu64 ", pagePID: %" PRIu64 ", contextID: %u", this, webPageProxyID.toUInt64(), pageID.toUInt64(), contextID);
    auto page = WebProcessProxy::webPage(webPageProxyID);
    if (!page) {
        RELEASE_LOG(Process, "%p - GPUProcessProxy::didCreateContextForVisibilityPropagation() No WebPageProxy with this identifier", this);
        return;
    }
    if (page->webPageIDInMainFrameProcess() == pageID) {
        page->didCreateContextInGPUProcessForVisibilityPropagation(contextID);
        return;
    }
    auto* provisionalPage = page->provisionalPageProxy();
    if (provisionalPage && provisionalPage->webPageID() == pageID) {
        provisionalPage->didCreateContextInGPUProcessForVisibilityPropagation(contextID);
        return;
    }
    RELEASE_LOG(Process, "%p - GPUProcessProxy::didCreateContextForVisibilityPropagation() There was a WebPageProxy for this identifier, but it had the wrong WebPage identifier.", this);
}
#endif

#if PLATFORM(MAC)
void GPUProcessProxy::setScreenProperties(const WebCore::ScreenProperties& properties)
{
    send(Messages::GPUProcess::SetScreenProperties { properties }, 0);
}
#endif

void GPUProcessProxy::updatePreferences(WebProcessProxy& webProcess)
{
    if (!canSendMessage())
        return;

    // FIXME (https://bugs.webkit.org/show_bug.cgi?id=241821): It's not ideal that these features are controlled by preferences-level feature flags (i.e. per-web view), but there is only
    // one GPU process and the runtime-enabled features backing these preferences are process-wide. We should refactor each of these features
    // so that they aren't process-global, and then reimplement this feature flag propagation to the GPU Process in a way that respects the
    // settings of the page that is hosting each media element.
    // For the time being, each of the below features are enabled in the GPU Process if it is enabled by at least one web page's preferences.
    // In practice, all web pages' preferences should agree on these feature flag values.
    GPUProcessPreferences gpuPreferences;
    for (Ref page : webProcess.pages()) {
        Ref webPreferences = page->preferences();
        if (!webPreferences->useGPUProcessForMediaEnabled())
            continue;
        gpuPreferences.copyEnabledWebPreferences(webPreferences);
    }
    
    send(Messages::GPUProcess::UpdateGPUProcessPreferences(gpuPreferences), 0);
}

void GPUProcessProxy::updateScreenPropertiesIfNeeded()
{
#if PLATFORM(MAC)
    if (!canSendMessage())
        return;

    setScreenProperties(collectScreenProperties());
#endif
}

void GPUProcessProxy::didBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::didBecomeUnresponsive: GPUProcess with PID %d became unresponsive, terminating it", this, processID());
    terminate();
    gpuProcessExited(ProcessTerminationReason::Unresponsive);
}

#if !PLATFORM(COCOA)
void GPUProcessProxy::platformInitializeGPUProcessParameters(GPUProcessCreationParameters& parameters)
{
}
#endif

#if ENABLE(VIDEO)
void GPUProcessProxy::requestBitmapImageForCurrentTime(ProcessIdentifier processIdentifier, MediaPlayerIdentifier playerIdentifier, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& completion)
{
    sendWithAsyncReply(Messages::GPUProcess::RequestBitmapImageForCurrentTime(processIdentifier, playerIdentifier), WTFMove(completion));
}
#endif

#if ENABLE(MEDIA_STREAM)
void GPUProcessProxy::voiceActivityDetected()
{
    for (Ref page : m_pagesListeningToVoiceActivity)
        page->voiceActivityDetected();
}

void GPUProcessProxy::startMonitoringCaptureDeviceRotation(PageIdentifier pageID, const String& persistentId)
{
    if (auto page = WebProcessProxy::webPage(pageID))
        page->startMonitoringCaptureDeviceRotation(persistentId);
}

void GPUProcessProxy::stopMonitoringCaptureDeviceRotation(PageIdentifier pageID, const String& persistentId)
{
    if (auto page = WebProcessProxy::webPage(pageID))
        page->stopMonitoringCaptureDeviceRotation(persistentId);
}

void GPUProcessProxy::rotationAngleForCaptureDeviceChanged(const String& persistentId, WebCore::VideoFrameRotation rotation)
{
    send(Messages::GPUProcess::RotationAngleForCaptureDeviceChanged(persistentId, rotation), 0);
}

void GPUProcessProxy::microphoneMuteStatusChanged(bool isMuting)
{
    // FIXME: We are currently muting/unmuting the last capturing page. If all pages are muted, maybe we should favor the currently focused page if it has muted microphone capture.
    ASSERT(m_lastPageUsingMicrophone);
    if (!m_lastPageUsingMicrophone)
        return;
    if (RefPtr page = WebProcessProxy::webPage(*m_lastPageUsingMicrophone))
        page->microphoneMuteStatusChanged(isMuting);
}

#if PLATFORM(IOS_FAMILY)
void GPUProcessProxy::statusBarWasTapped(CompletionHandler<void()>&& completionHandler)
{
    if (auto page = WebProcessProxy::audioCapturingWebPage())
        page->statusBarWasTapped();
    // Find the web page capturing audio and put focus on it.
    completionHandler();
}
#endif
#endif // ENABLE(MEDIA_STREAM)

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
