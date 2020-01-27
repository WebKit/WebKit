/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "AuxiliaryProcess.h"

#include "ContentWorldShared.h"
#include "Logging.h"
#include "SandboxInitializationParameters.h"
#include <pal/SessionID.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if OS(LINUX)
#include <wtf/MemoryPressureHandler.h>
#endif

namespace WebKit {
using namespace WebCore;

AuxiliaryProcess::AuxiliaryProcess()
    : m_terminationCounter(0)
    , m_terminationTimer(RunLoop::main(), this, &AuxiliaryProcess::terminationTimerFired)
    , m_processSuppressionDisabled("Process Suppression Disabled by UIProcess")
{
}

AuxiliaryProcess::~AuxiliaryProcess()
{
}

void AuxiliaryProcess::didClose(IPC::Connection&)
{
    _exit(EXIT_SUCCESS);
}

void AuxiliaryProcess::initialize(const AuxiliaryProcessInitializationParameters& parameters)
{
    WTF::RefCountedBase::enableThreadingChecksGlobally();

    RELEASE_ASSERT_WITH_MESSAGE(parameters.processIdentifier, "Unable to initialize child process without a WebCore process identifier");
    Process::setIdentifier(*parameters.processIdentifier);

    platformInitialize();

#if PLATFORM(COCOA)
    m_priorityBoostMessage = parameters.priorityBoostMessage;
#endif

    initializeProcess(parameters);

    SandboxInitializationParameters sandboxParameters;
    initializeSandbox(parameters, sandboxParameters);

    initializeProcessName(parameters);

    // In WebKit2, only the UI process should ever be generating certain identifiers.
    PAL::SessionID::enableGenerationProtection();
    ContentWorldIdentifier::enableGenerationProtection();

    m_connection = IPC::Connection::createClientConnection(parameters.connectionIdentifier, *this);
    initializeConnection(m_connection.get());
    m_connection->open();
}

void AuxiliaryProcess::setProcessSuppressionEnabled(bool enabled)
{
    if (enabled)
        m_processSuppressionDisabled.stop();
    else
        m_processSuppressionDisabled.start();
}

void AuxiliaryProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
}

void AuxiliaryProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
}

void AuxiliaryProcess::initializeConnection(IPC::Connection*)
{
}

void AuxiliaryProcess::addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void AuxiliaryProcess::addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void AuxiliaryProcess::removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

void AuxiliaryProcess::removeMessageReceiver(IPC::StringReference messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

void AuxiliaryProcess::removeMessageReceiver(IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiver);
}

void AuxiliaryProcess::disableTermination()
{
    m_terminationCounter++;
    m_terminationTimer.stop();
}

void AuxiliaryProcess::enableTermination()
{
    ASSERT(m_terminationCounter > 0);
    m_terminationCounter--;

    if (m_terminationCounter)
        return;

    if (!m_terminationTimeout) {
        terminationTimerFired();
        return;
    }

    m_terminationTimer.startOneShot(m_terminationTimeout);
}

IPC::Connection* AuxiliaryProcess::messageSenderConnection() const
{
    return m_connection.get();
}

uint64_t AuxiliaryProcess::messageSenderDestinationID() const
{
    return 0;
}

void AuxiliaryProcess::terminationTimerFired()
{
    if (!shouldTerminate())
        return;

    terminate();
}

void AuxiliaryProcess::stopRunLoop()
{
    platformStopRunLoop();
}

#if !PLATFORM(COCOA)
void AuxiliaryProcess::platformStopRunLoop()
{
    RunLoop::main().stop();
}
#endif

void AuxiliaryProcess::terminate()
{
    m_connection->invalidate();

    stopRunLoop();
}

void AuxiliaryProcess::shutDown()
{
    terminate();
}

Optional<std::pair<IPC::Connection::Identifier, IPC::Attachment>> AuxiliaryProcess::createIPCConnectionPair()
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();
    return std::make_pair(socketPair.server, IPC::Attachment { socketPair.client });
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(Process, "AuxiliaryProcess::createIPCConnectionPair: Could not allocate mach port, error %x", kr);
        CRASH();
    }
    if (!MACH_PORT_VALID(listeningPort)) {
        RELEASE_LOG_ERROR(Process, "AuxiliaryProcess::createIPCConnectionPair: Could not allocate mach port, returned port was invalid");
        CRASH();
    }
    return std::make_pair(IPC::Connection::Identifier { listeningPort }, IPC::Attachment { listeningPort, MACH_MSG_TYPE_MAKE_SEND });
#elif OS(WINDOWS)
    IPC::Connection::Identifier serverIdentifier, clientIdentifier;
    if (!IPC::Connection::createServerAndClientIdentifiers(serverIdentifier, clientIdentifier)) {
        LOG_ERROR("Failed to create server and client identifiers");
        CRASH();
    }
    return std::make_pair(serverIdentifier, IPC::Attachment { clientIdentifier });
#else
    notImplemented();
    return { };
#endif
}

#if !PLATFORM(COCOA)
void AuxiliaryProcess::platformInitialize()
{
}

void AuxiliaryProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void AuxiliaryProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
    WTFLogAlways("Received invalid message: '%s::%s'", messageReceiverName.toString().data(), messageName.toString().data());
    CRASH();
}

#if OS(LINUX)
void AuxiliaryProcess::didReceiveMemoryPressureEvent(bool isCritical)
{
    MemoryPressureHandler::singleton().triggerMemoryPressureEvent(isCritical);
}
#endif

#endif // !PLATFORM(COCOA)

} // namespace WebKit
