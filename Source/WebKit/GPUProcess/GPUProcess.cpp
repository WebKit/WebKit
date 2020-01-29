/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "GPUProcess.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "AuxiliaryProcessMessages.h"
#include "DataReference.h"
#include "GPUConnectionToWebProcess.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessSessionParameters.h"
#include "Logging.h"
#include "SandboxExtension.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include <WebCore/LogInitialization.h>
#include <WebCore/MockAudioSharedUnit.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/AtomString.h>

namespace WebKit {
using namespace WebCore;


GPUProcess::GPUProcess(AuxiliaryProcessInitializationParameters&& parameters)
{
    initialize(WTFMove(parameters));
}

GPUProcess::~GPUProcess()
{
}

void GPUProcess::createGPUConnectionToWebProcess(ProcessIdentifier identifier, PAL::SessionID sessionID, CompletionHandler<void(Optional<IPC::Attachment>&&)>&& completionHandler)
{
    auto ipcConnection = createIPCConnectionPair();
    if (!ipcConnection) {
        completionHandler({ });
        return;
    }

    auto newConnection = GPUConnectionToWebProcess::create(*this, identifier, ipcConnection->first, sessionID);

    ASSERT(!m_webProcessConnections.contains(identifier));
    m_webProcessConnections.add(identifier, WTFMove(newConnection));

    completionHandler(WTFMove(ipcConnection->second));
}

void GPUProcess::removeGPUConnectionToWebProcess(GPUConnectionToWebProcess& connection)
{
    ASSERT(m_webProcessConnections.contains(connection.webProcessIdentifier()));
    m_webProcessConnections.remove(connection.webProcessIdentifier());
}

void GPUProcess::connectionToWebProcessClosed(IPC::Connection& connection)
{
}

bool GPUProcess::shouldTerminate()
{
    return m_webProcessConnections.isEmpty();
}

void GPUProcess::didClose(IPC::Connection&)
{
    ASSERT(RunLoop::isMain());
}

void GPUProcess::lowMemoryHandler(Critical critical)
{
    WTF::releaseFastMallocFreeMemory();
}

void GPUProcess::initializeGPUProcess(GPUProcessCreationParameters&& parameters)
{
    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomString::init();

#if ENABLE(MEDIA_STREAM)
    setMockCaptureDevicesEnabled(parameters.useMockCaptureDevices);
    SandboxExtension::consumePermanently(parameters.cameraSandboxExtensionHandle);
    SandboxExtension::consumePermanently(parameters.microphoneSandboxExtensionHandle);
#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.tccSandboxExtensionHandle);
#endif
#endif
}

void GPUProcess::prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcess::prepareToSuspend(), isSuspensionImminent: %d", this, isSuspensionImminent);

    lowMemoryHandler(Critical::Yes);
}

void GPUProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcess::processDidResume()", this);
    resume();
}

void GPUProcess::resume()
{
}

void GPUProcess::processDidTransitionToForeground()
{
}

void GPUProcess::processDidTransitionToBackground()
{
}

GPUConnectionToWebProcess* GPUProcess::webProcessConnection(ProcessIdentifier identifier) const
{
    return m_webProcessConnections.get(identifier);
}

void GPUProcess::setMockCaptureDevicesEnabled(bool isEnabled)
{
#if ENABLE(MEDIA_STREAM)
    // FIXME: Enable the audio session check by implementing an AudioSession for the GPUProcess.
    MockAudioSharedUnit::singleton().setDisableAudioSessionCheck(isEnabled);
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(isEnabled);
#endif
}

void GPUProcess::addSession(PAL::SessionID sessionID, GPUProcessSessionParameters&& parameters)
{
    ASSERT(!m_sessions.contains(sessionID));
    SandboxExtension::consumePermanently(parameters.mediaCacheDirectorySandboxExtensionHandle);
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    SandboxExtension::consumePermanently(parameters.mediaKeysStorageDirectorySandboxExtensionHandle);
#endif

    m_sessions.add(sessionID, GPUSession {
        WTFMove(parameters.mediaCacheDirectory)
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        , WTFMove(parameters.mediaKeysStorageDirectory)
#endif
    });
}

void GPUProcess::removeSession(PAL::SessionID sessionID)
{
    ASSERT(m_sessions.contains(sessionID));
    m_sessions.remove(sessionID);
}

const String& GPUProcess::mediaCacheDirectory(PAL::SessionID sessionID) const
{
    ASSERT(m_sessions.contains(sessionID));
    return m_sessions.find(sessionID)->value.mediaCacheDirectory;
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
const String& GPUProcess::mediaKeysStorageDirectory(PAL::SessionID sessionID) const
{
    ASSERT(m_sessions.contains(sessionID));
    return m_sessions.find(sessionID)->value.mediaKeysStorageDirectory;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
