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
#include "ChildProcess.h"

#include "Logging.h"
#include "SandboxInitializationParameters.h"
#include <WebCore/SchemeRegistry.h>
#include <pal/SessionID.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if OS(LINUX)
#include <wtf/MemoryPressureHandler.h>
#endif

using namespace WebCore;

namespace WebKit {

ChildProcess::ChildProcess()
    : m_terminationCounter(0)
    , m_terminationTimer(RunLoop::main(), this, &ChildProcess::terminationTimerFired)
    , m_processSuppressionDisabled("Process Suppression Disabled by UIProcess")
{
}

ChildProcess::~ChildProcess()
{
}

void ChildProcess::didClose(IPC::Connection&)
{
    _exit(EXIT_SUCCESS);
}

void ChildProcess::initialize(const ChildProcessInitializationParameters& parameters)
{
    RELEASE_ASSERT_WITH_MESSAGE(parameters.processIdentifier, "Unable to initialize child process without a WebCore process identifier");
    Process::setIdentifier(*parameters.processIdentifier);

    platformInitialize();

#if PLATFORM(COCOA)
    m_priorityBoostMessage = parameters.priorityBoostMessage;
#endif

    initializeProcess(parameters);
    initializeProcessName(parameters);

    SandboxInitializationParameters sandboxParameters;
    initializeSandbox(parameters, sandboxParameters);

    // In WebKit2, only the UI process should ever be generating non-default PAL::SessionIDs.
    PAL::SessionID::enableGenerationProtection();

    m_connection = IPC::Connection::createClientConnection(parameters.connectionIdentifier, *this);
    initializeConnection(m_connection.get());
    m_connection->open();
}

void ChildProcess::setProcessSuppressionEnabled(bool enabled)
{
    if (enabled)
        m_processSuppressionDisabled.stop();
    else
        m_processSuppressionDisabled.start();
}

void ChildProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void ChildProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void ChildProcess::initializeConnection(IPC::Connection*)
{
}

void ChildProcess::addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void ChildProcess::addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void ChildProcess::removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

void ChildProcess::removeMessageReceiver(IPC::StringReference messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

void ChildProcess::removeMessageReceiver(IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiver);
}

void ChildProcess::disableTermination()
{
    m_terminationCounter++;
    m_terminationTimer.stop();
}

void ChildProcess::enableTermination()
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

IPC::Connection* ChildProcess::messageSenderConnection()
{
    return m_connection.get();
}

uint64_t ChildProcess::messageSenderDestinationID()
{
    return 0;
}

void ChildProcess::terminationTimerFired()
{
    if (!shouldTerminate())
        return;

    terminate();
}

void ChildProcess::stopRunLoop()
{
    platformStopRunLoop();
}

#if !PLATFORM(IOS_FAMILY)
void ChildProcess::platformStopRunLoop()
{
    RunLoop::main().stop();
}
#endif

void ChildProcess::terminate()
{
    m_connection->invalidate();

    stopRunLoop();
}

void ChildProcess::shutDown()
{
    terminate();
}

void ChildProcess::registerURLSchemeServiceWorkersCanHandle(const String& urlScheme) const
{
    WebCore::SchemeRegistry::registerURLSchemeServiceWorkersCanHandle(urlScheme);
}

#if !PLATFORM(COCOA)
void ChildProcess::platformInitialize()
{
}

void ChildProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void ChildProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
    WTFLogAlways("Received invalid message: '%s::%s'", messageReceiverName.toString().data(), messageName.toString().data());
    CRASH();
}

#if OS(LINUX)
void ChildProcess::didReceiveMemoryPressureEvent(bool isCritical)
{
    MemoryPressureHandler::singleton().triggerMemoryPressureEvent(isCritical);
}
#endif

#endif // !PLATFORM(COCOA)

} // namespace WebKit
