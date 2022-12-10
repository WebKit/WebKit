/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "ProvisionalPageProxy.h"
#include "WebPageGroup.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/LogInitialization.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/ScreenProperties.h>
#include <wtf/CompletionHandler.h>
#include <wtf/LogInitialization.h>
#include <wtf/MachSendRight.h>
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
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, this->connection())

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

Ref<GPUProcessProxy> GPUProcessProxy::getOrCreate()
{
    ASSERT(RunLoop::isMain());
    if (auto& existingGPUProcess = singleton()) {
        ASSERT(existingGPUProcess->state() != State::Terminated);
        return *existingGPUProcess;
    }
    auto gpuProcess = adoptRef(*new GPUProcessProxy);
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
    String path = WebsiteDataStore::cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.GPU/"_s);

    FileSystem::makeAllDirectories(path);
    
    return path;
}
#endif

GPUProcessProxy::GPUProcessProxy()
    : AuxiliaryProcessProxy()
    , m_throttler(*this, WebProcessPool::anyProcessPoolNeedsUIBackgroundAssertion())
#if ENABLE(MEDIA_STREAM)
    , m_useMockCaptureDevices(MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled())
#endif
{
    connect();

    GPUProcessCreationParameters parameters;
    parameters.auxiliaryProcessParameters = auxiliaryProcessParameters();

#if ENABLE(MEDIA_STREAM)
    parameters.useMockCaptureDevices = m_useMockCaptureDevices;
#if PLATFORM(MAC)
    // FIXME: Remove this and related parameter when <rdar://problem/29448368> is fixed.
    if (MacApplication::isSafari()) {
        if (auto handle = SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone"_s))
            parameters.microphoneSandboxExtensionHandle = WTFMove(*handle);
        m_hasSentMicrophoneSandboxExtension = true;
    }
#endif
#endif // ENABLE(MEDIA_STREAM)

    parameters.parentPID = getCurrentProcessID();

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    auto containerCachesDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(gpuProcessCachesDirectory());
    auto containerTemporaryDirectory = WebsiteDataStore::defaultResolvedContainerTemporaryDirectory();

    if (!containerCachesDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(containerCachesDirectory, SandboxExtension::Type::ReadWrite))
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

    platformInitializeGPUProcessParameters(parameters);

    // Initialize the GPU process.
    send(Messages::GPUProcess::InitializeGPUProcess(parameters), 0);

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

void GPUProcessProxy::setOrientationForMediaCapture(uint64_t orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    send(Messages::GPUProcess::SetOrientationForMediaCapture { orientation }, 0);
}

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
#if HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
            auto additionalAppleCameraServicePathSandboxExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.appleh13camerad"_s, std::nullopt);
            if (!additionalAppleCameraServicePathSandboxExtensionHandle) {
                RELEASE_LOG_ERROR(WebRTC, "Unable to create com.apple.appleh13camerad sandbox extension");
                return false;
            }
            extensions.append(WTFMove(*additionalAppleCameraServicePathSandboxExtensionHandle));
#endif
            extensions.append(WTFMove(*appleCameraServicePathSandboxExtensionHandle));
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

#if HAVE(SC_CONTENT_SHARING_SESSION)
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

#if PLATFORM(IOS)
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

#if HAVE(SC_CONTENT_SHARING_SESSION)
    if (allowDisplayCapture && !m_hasSentDisplayCaptureSandboxExtension && addDisplayCaptureSandboxExtension(connection()->getAuditToken(), extensions))
        m_hasSentDisplayCaptureSandboxExtension = true;
#endif

#if PLATFORM(IOS)
    if ((allowAudioCapture || allowVideoCapture) && !m_hasSentTCCDSandboxExtension && addTCCDSandboxExtension(extensions))
        m_hasSentTCCDSandboxExtension = true;
#endif // PLATFORM(IOS)

    if (!extensions.isEmpty())
        send(Messages::GPUProcess::UpdateSandboxAccess { extensions }, 0);
#endif // PLATFORM(COCOA)
}

void GPUProcessProxy::updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier processID, CompletionHandler<void()>&& completionHandler)
{
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

void GPUProcessProxy::triggerMockMicrophoneConfigurationChange()
{
    send(Messages::GPUProcess::TriggerMockMicrophoneConfigurationChange { }, 0);
}
#endif // ENABLE(MEDIA_STREAM)

#if HAVE(SC_CONTENT_SHARING_SESSION)
void GPUProcessProxy::showWindowPicker(CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::ShowWindowPicker { }, WTFMove(completionHandler));
}

void GPUProcessProxy::showScreenPicker(CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::ShowScreenPicker { }, WTFMove(completionHandler));
}
#endif

void GPUProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::GPU;
    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);
}

void GPUProcessProxy::connectionWillOpen(IPC::Connection&)
{
}

void GPUProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
    if (singleton() == this)
        singleton() = nullptr;
}

#if ENABLE(VP9)
std::optional<bool> GPUProcessProxy::s_hasVP9HardwareDecoder;
std::optional<bool> GPUProcessProxy::s_hasVP9ExtensionSupport;
#endif

void GPUProcessProxy::createGPUProcessConnection(WebProcessProxy& webProcessProxy, IPC::Connection::Handle&& connectionIdentifier, GPUProcessConnectionParameters&& parameters)
{
#if ENABLE(VP9)
    parameters.hasVP9HardwareDecoder = s_hasVP9HardwareDecoder;
    parameters.hasVP9ExtensionSupport = s_hasVP9ExtensionSupport;
#endif

    if (auto* store = webProcessProxy.websiteDataStore())
        addSession(*store);

    RELEASE_LOG(ProcessSuspension, "%p - GPUProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    startResponsivenessTimer(UseLazyStop::No);
    sendWithAsyncReply(Messages::GPUProcess::CreateGPUConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID(), WTFMove(connectionIdentifier), parameters }, [this, weakThis = WeakPtr { *this }]() mutable {
        if (!weakThis)
            return;
        stopResponsivenessTimer();
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
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
        RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::gpuProcessExited: reason=%" PUBLIC_LOG_STRING, this, processTerminationReasonToString(reason));
        break;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
        ASSERT_NOT_REACHED();
        break;
    }

    if (singleton() == this)
        singleton() = nullptr;

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->gpuProcessExited(processIdentifier(), reason);
}

void GPUProcessProxy::processIsReadyToExit()
{
    RELEASE_LOG(Process, "%p - GPUProcessProxy::processIsReadyToExit:", this);
    terminate();
    gpuProcessExited(ProcessTerminationReason::IdleExit); // May cause |this| to get deleted.
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

void GPUProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
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
    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!connectionIdentifier) {
        gpuProcessExited(ProcessTerminationReason::Crash);
        return;
    }
    
#if USE(RUNNINGBOARD)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
#endif

#if PLATFORM(COCOA)
    if (auto networkProcess = NetworkProcessProxy::defaultNetworkProcess())
        networkProcess->sendXPCEndpointToProcess(*this);
#endif

    beginResponsivenessChecks();

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->gpuProcessDidFinishLaunching(processIdentifier());

#if HAVE(POWERLOG_TASK_MODE_QUERY)
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([weakThis = WeakPtr { *this }] () mutable {
        if (!isPowerLoggingInTaskMode())
            return;
        RunLoop::main().dispatch([weakThis = WTFMove(weakThis)] () {
            if (!weakThis)
                return;
            weakThis->enablePowerLogging();
        });
    }).get());
#endif
}

void GPUProcessProxy::updateProcessAssertion()
{
    bool hasAnyForegroundWebProcesses = false;
    bool hasAnyBackgroundWebProcesses = false;

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        hasAnyForegroundWebProcesses |= processPool->hasForegroundWebProcesses();
        hasAnyBackgroundWebProcesses |= processPool->hasBackgroundWebProcesses();
    }

    if (hasAnyForegroundWebProcesses) {
        if (!ProcessThrottler::isValidForegroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().foregroundActivity("GPU for foreground view(s)"_s);
        }
        return;
    }
    if (hasAnyBackgroundWebProcesses) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().backgroundActivity("GPU for background view(s)"_s);
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

    parameters.mediaCacheDirectory = store.resolvedMediaCacheDirectory();
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    if (!parameters.mediaCacheDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaCacheDirectory, SandboxExtension::Type::ReadWrite))
            parameters.mediaCacheDirectorySandboxExtensionHandle = WTFMove(*handle);
    }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    parameters.mediaKeysStorageDirectory = store.resolvedMediaKeysDirectory();
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
    sendWithAsyncReply(Messages::GPUProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes, MonotonicTime::now() + Seconds(remainingRunTime)), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void GPUProcessProxy::sendProcessDidResume(ResumeReason)
{
    if (canSendMessage())
        send(Messages::GPUProcess::ProcessDidResume(), 0);
}

void GPUProcessProxy::terminateWebProcess(WebCore::ProcessIdentifier webProcessIdentifier)
{
    if (auto* process = WebProcessProxy::processForIdentifier(webProcessIdentifier))
        process->requestTermination(ProcessTerminationReason::RequestedByGPUProcess);
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void GPUProcessProxy::didCreateContextForVisibilityPropagation(WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier pageID, LayerHostingContextID contextID)
{
    RELEASE_LOG(Process, "GPUProcessProxy::didCreateContextForVisibilityPropagation: webPageProxyID: %" PRIu64 ", pagePID: %" PRIu64 ", contextID: %u", webPageProxyID.toUInt64(), pageID.toUInt64(), contextID);
    auto page = WebProcessProxy::webPage(webPageProxyID);
    if (!page) {
        RELEASE_LOG(Process, "GPUProcessProxy::didCreateContextForVisibilityPropagation() No WebPageProxy with this identifier");
        return;
    }
    if (page->webPageID() == pageID) {
        page->didCreateContextInGPUProcessForVisibilityPropagation(contextID);
        return;
    }
    auto* provisionalPage = page->provisionalPageProxy();
    if (provisionalPage && provisionalPage->webPageID() == pageID) {
        provisionalPage->didCreateContextInGPUProcessForVisibilityPropagation(contextID);
        return;
    }
    RELEASE_LOG(Process, "GPUProcessProxy::didCreateContextForVisibilityPropagation() There was a WebPageProxy for this identifier, but it had the wrong WebPage identifier.");
}
#endif

#if PLATFORM(MAC)
void GPUProcessProxy::displayConfigurationChanged(CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags)
{
    send(Messages::GPUProcess::DisplayConfigurationChanged { displayID, flags }, 0);
}

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
    for (auto page : webProcess.pages()) {
        auto& webPreferences = page->preferences();
        if (!webPreferences.useGPUProcessForMediaEnabled())
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
    RELEASE_LOG_ERROR(Process, "GPUProcessProxy::didBecomeUnresponsive: GPUProcess with PID %d became unresponsive, terminating it", processIdentifier());
    terminate();
    gpuProcessExited(ProcessTerminationReason::Unresponsive);
}

#if !PLATFORM(COCOA)
void GPUProcessProxy::platformInitializeGPUProcessParameters(GPUProcessCreationParameters& parameters)
{
}
#endif

void GPUProcessProxy::requestBitmapImageForCurrentTime(ProcessIdentifier processIdentifier, MediaPlayerIdentifier playerIdentifier, CompletionHandler<void(const ShareableBitmapHandle&)>&& completion)
{
    sendWithAsyncReply(Messages::GPUProcess::RequestBitmapImageForCurrentTime(processIdentifier, playerIdentifier), WTFMove(completion));
}

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
void GPUProcessProxy::statusBarWasTapped(CompletionHandler<void()>&& completionHandler)
{
    if (auto page = WebProcessProxy::audioCapturingWebPage())
        page->statusBarWasTapped();
    // Find the web page capturing audio and put focus on it.
    completionHandler();
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
