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
#include "GPUProcessConnectionInfo.h"
#include "GPUProcessConnectionParameters.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessMessages.h"
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
#include <wtf/CompletionHandler.h>
#include <wtf/LogInitialization.h>
#include <wtf/TranslatedProcess.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/AGXCompilerService.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
#include "SandboxUtilities.h"
#include <wtf/FileSystem.h>
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

#if PLATFORM(IOS_FAMILY)
static const Vector<ASCIILiteral>& nonBrowserServices()
{
    ASSERT(isMainRunLoop());
    static const auto services = makeNeverDestroyed(Vector<ASCIILiteral> {
        "com.apple.iconservices"_s,
        "com.apple.PowerManagement.control"_s,
        "com.apple.frontboard.systemappservices"_s
    });
    return services;
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
    gpuProcess->updatePreferences();
    singleton() = makeWeakPtr(gpuProcess.get());
    return gpuProcess;
}

GPUProcessProxy* GPUProcessProxy::singletonIfCreated()
{
    return singleton().get();
}

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
static String gpuProcessCachesDirectory()
{
    String path = WebProcessPool::cacheDirectoryInContainerOrHomeDirectory("/Library/Caches/com.apple.WebKit.GPU/"_s);

    FileSystem::makeAllDirectories(path);
    
    return path;
}
#endif

GPUProcessProxy::GPUProcessProxy()
    : AuxiliaryProcessProxy()
    , m_throttler(*this, false)
#if ENABLE(MEDIA_STREAM)
    , m_useMockCaptureDevices(MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled())
#endif
{
    connect();

    GPUProcessCreationParameters parameters;
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
    auto containerTemporaryDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(WebProcessPool::containerTemporaryDirectory());

    if (!containerCachesDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(containerCachesDirectory, SandboxExtension::Type::ReadWrite))
            parameters.containerCachesDirectoryExtensionHandle = WTFMove(*handle);
    }

    if (!containerTemporaryDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(containerTemporaryDirectory, SandboxExtension::Type::ReadWrite))
            parameters.containerTemporaryDirectoryExtensionHandle = WTFMove(*handle);
    }
#endif
#if PLATFORM(IOS_FAMILY)
    if (WebCore::deviceHasAGXCompilerService()) {
        parameters.compilerServiceExtensionHandles = SandboxExtension::createHandlesForMachLookup(WebCore::agxCompilerServices(), std::nullopt);
        parameters.dynamicIOKitExtensionHandles = SandboxExtension::createHandlesForIOKitClassExtensions(WebCore::agxCompilerClasses(), std::nullopt);
    }

    if (!WebCore::IOSApplication::isMobileSafari())
        parameters.dynamicMachExtensionHandles = SandboxExtension::createHandlesForMachLookup(nonBrowserServices(), std::nullopt);
#endif

    platformInitializeGPUProcessParameters(parameters);

    // Initialize the GPU process.
    send(Messages::GPUProcess::InitializeGPUProcess(parameters), 0);
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

void GPUProcessProxy::updateSandboxAccess(bool allowAudioCapture, bool allowVideoCapture)
{
    if (m_useMockCaptureDevices)
        return;

#if PLATFORM(COCOA)
    Vector<SandboxExtension::Handle> extensions;

    if (allowVideoCapture && !m_hasSentCameraSandboxExtension && addCameraSandboxExtensions(extensions))
        m_hasSentCameraSandboxExtension = true;

    if (allowAudioCapture && !m_hasSentMicrophoneSandboxExtension && addMicrophoneSandboxExtension(extensions))
        m_hasSentMicrophoneSandboxExtension = true;

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
    updateSandboxAccess(allowAudioCapture, allowVideoCapture);
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

void GPUProcessProxy::resetMockMediaDevices()
{
    send(Messages::GPUProcess::ResetMockMediaDevices { }, 0);
}

void GPUProcessProxy::setMockCameraIsInterrupted(bool isInterrupted)
{
    send(Messages::GPUProcess::SetMockCameraIsInterrupted { isInterrupted }, 0);
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

void GPUProcessProxy::getGPUProcessConnection(WebProcessProxy& webProcessProxy, const GPUProcessConnectionParameters& parameters, Messages::WebProcessProxy::GetGPUProcessConnection::DelayedReply&& reply)
{
    addSession(webProcessProxy.websiteDataStore());

    RELEASE_LOG(ProcessSuspension, "%p - GPUProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    startResponsivenessTimer(UseLazyStop::No);
    sendWithAsyncReply(Messages::GPUProcess::CreateGPUConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID(), parameters }, [this, weakThis = makeWeakPtr(*this), reply = WTFMove(reply)](auto&& identifier, auto&& connectionParameters) mutable {
        if (!weakThis) {
            RELEASE_LOG_ERROR(Process, "GPUProcessProxy::getGPUProcessConnection: GPUProcessProxy deallocated during connection establishment");
            return reply({ });
        }

        stopResponsivenessTimer();
        if (!identifier) {
            RELEASE_LOG_ERROR(Process, "GPUProcessProxy::getGPUProcessConnection: connection identifier is empty");
            return reply({ });
        }

#if USE(UNIX_DOMAIN_SOCKETS) || OS(WINDOWS)
        reply(GPUProcessConnectionInfo { WTFMove(*identifier) });
        UNUSED_VARIABLE(this);
#elif OS(DARWIN)
        MESSAGE_CHECK(MACH_PORT_VALID(identifier->port()));
        reply(GPUProcessConnectionInfo { IPC::Attachment { identifier->port(), MACH_MSG_TYPE_MOVE_SEND }, this->connection()->getAuditToken(), WTFMove(connectionParameters) });
#else
        notImplemented();
#endif
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void GPUProcessProxy::gpuProcessExited(GPUProcessTerminationReason reason)
{
    Ref protectedThis { *this };

    switch (reason) {
    case GPUProcessTerminationReason::Crash:
        RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::gpuProcessExited: reason=crash", this);
        break;
    case GPUProcessTerminationReason::IdleExit:
        RELEASE_LOG(Process, "%p - GPUProcessProxy::gpuProcessExited: reason=idle-exit", this);
        break;
    case GPUProcessTerminationReason::Unresponsive:
        RELEASE_LOG(Process, "%p - GPUProcessProxy::gpuProcessExited: reason=unresponsive", this);
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
    gpuProcessExited(GPUProcessTerminationReason::IdleExit); // May cause |this| to get deleted.
}

void GPUProcessProxy::terminateForTesting()
{
    processIsReadyToExit();
}

void GPUProcessProxy::didClose(IPC::Connection&)
{
    RELEASE_LOG_ERROR(Process, "%p - GPUProcessProxy::didClose:", this);
    gpuProcessExited(GPUProcessTerminationReason::Crash); // May cause |this| to get deleted.
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

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        gpuProcessExited(GPUProcessTerminationReason::Crash);
        return;
    }
    
#if PLATFORM(IOS_FAMILY)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
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

void GPUProcessProxy::sendPrepareToSuspend(IsSuspensionImminent isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void GPUProcessProxy::sendProcessDidResume()
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
    RELEASE_LOG(Process, "GPUProcessProxy::didCreateContextForVisibilityPropagation: webPageProxyID: %" PRIu64 ", pagePID: %" PRIu64 ", contextID: %d", webPageProxyID.toUInt64(), pageID.toUInt64(), contextID);
    auto* page = WebProcessProxy::webPage(webPageProxyID);
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
#endif

void GPUProcessProxy::updatePreferences()
{
    if (!canSendMessage())
        return;

#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
    bool hasEnabledWebMParser = false;
#endif

#if ENABLE(WEBM_FORMAT_READER)
    bool hasEnabledWebMFormatReader = false;
#endif

#if ENABLE(OPUS)
    bool hasEnabledOpus = false;
#endif

#if ENABLE(VORBIS)
    bool hasEnabledVorbis = false;
#endif

    WebPageGroup::forEach([&] (auto& group) mutable {
        if (!group.preferences().useGPUProcessForMediaEnabled())
            return;

#if ENABLE(OPUS)
        if (group.preferences().opusDecoderEnabled())
            hasEnabledOpus = true;
#endif

#if ENABLE(VORBIS)
        if (group.preferences().vorbisDecoderEnabled())
            hasEnabledVorbis = true;
#endif

#if ENABLE(WEBM_FORMAT_READER)
        if (group.preferences().webMFormatReaderEnabled())
            hasEnabledWebMFormatReader = true;
#endif

#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
        if (group.preferences().webMParserEnabled())
            hasEnabledWebMParser = true;
#endif
    });

#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
    send(Messages::GPUProcess::SetWebMParserEnabled(hasEnabledWebMParser), 0);
#endif

#if ENABLE(WEBM_FORMAT_READER)
    send(Messages::GPUProcess::SetWebMFormatReaderEnabled(hasEnabledWebMFormatReader), 0);
#endif

#if ENABLE(OPUS)
    send(Messages::GPUProcess::SetOpusDecoderEnabled(hasEnabledOpus), 0);
#endif

#if ENABLE(VORBIS)
    send(Messages::GPUProcess::SetVorbisDecoderEnabled(hasEnabledVorbis), 0);
#endif
}

void GPUProcessProxy::didBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "GPUProcessProxy::didBecomeUnresponsive: GPUProcess with PID %d became unresponsive, terminating it", processIdentifier());
    terminate();
    gpuProcessExited(GPUProcessTerminationReason::Unresponsive);
}

#if !PLATFORM(COCOA)
void GPUProcessProxy::platformInitializeGPUProcessParameters(GPUProcessCreationParameters& parameters)
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    parameters.wtfLoggingChannels = WTF::logLevelString();
    parameters.webCoreLoggingChannels = WebCore::logLevelString();
    parameters.webKitLoggingChannels = WebKit::logLevelString();
#endif
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
