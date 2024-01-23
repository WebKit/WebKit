/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ModelConnectionToWebProcess.h"

#if ENABLE(MODEL_PROCESS)

#include "DataReference.h"
#include "LayerHostingContext.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "ModelConnectionToWebProcessMessages.h"
#include "ModelProcess.h"
#include "ModelProcessConnectionInfo.h"
#include "ModelProcessConnectionMessages.h"
#include "ModelProcessConnectionParameters.h"
#include "ModelProcessMessages.h"
#include "ModelProcessProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebProcessMessages.h"
#include <WebCore/LogInitialization.h>

#if ENABLE(IPC_TESTING_API)
#include "IPCTesterMessages.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&connection()))

namespace WebKit {
using namespace WebCore;

Ref<ModelConnectionToWebProcess> ModelConnectionToWebProcess::create(ModelProcess& modelProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, IPC::Connection::Handle&& connectionHandle, ModelProcessConnectionParameters&& parameters)
{
    return adoptRef(*new ModelConnectionToWebProcess(modelProcess, webProcessIdentifier, sessionID, WTFMove(connectionHandle), WTFMove(parameters)));
}

ModelConnectionToWebProcess::ModelConnectionToWebProcess(ModelProcess& modelProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, IPC::Connection::Handle&& connectionHandle, ModelProcessConnectionParameters&& parameters)
    : m_connection(IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(connectionHandle) }))
    , m_modelProcess(modelProcess)
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_webProcessIdentity(WTFMove(parameters.webProcessIdentity))
    , m_sessionID(sessionID)
#if HAVE(AUDIT_TOKEN)
    , m_presentingApplicationAuditToken(parameters.presentingApplicationAuditToken ? std::optional(parameters.presentingApplicationAuditToken->auditToken()) : std::nullopt)
#endif
{
    RELEASE_ASSERT(RunLoop::isMain());

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the model process, which would be unsafe.
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open(*this);

    WebKit::ModelProcessConnectionInfo info {
#if HAVE(AUDIT_TOKEN)
        modelProcess.parentProcessConnection()->getAuditToken(),
#endif
    };
    m_connection->send(Messages::ModelProcessConnection::DidInitialize(info), 0);
    ++gObjectCountForTesting;
}

ModelConnectionToWebProcess::~ModelConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();

    --gObjectCountForTesting;
}

uint64_t ModelConnectionToWebProcess::gObjectCountForTesting = 0;

void ModelConnectionToWebProcess::didClose(IPC::Connection& connection)
{
    modelProcess().connectionToWebProcessClosed(connection);
    modelProcess().removeModelConnectionToWebProcess(*this); // May destroy |this|.
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void ModelConnectionToWebProcess::createVisibilityPropagationContextForPage(WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier pageID, bool canShowWhileLocked)
{
    auto contextForVisibilityPropagation = LayerHostingContext::createForExternalHostingProcess({ canShowWhileLocked });
    RELEASE_LOG(Process, "ModelConnectionToWebProcess::createVisibilityPropagationContextForPage: pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", contextID=%u", pageProxyID.toUInt64(), pageID.toUInt64(), contextForVisibilityPropagation->contextID());
    modelProcess().send(Messages::ModelProcessProxy::DidCreateContextForVisibilityPropagation(pageProxyID, pageID, contextForVisibilityPropagation->contextID()));
    m_visibilityPropagationContexts.add(std::make_pair(pageProxyID, pageID), WTFMove(contextForVisibilityPropagation));
}

void ModelConnectionToWebProcess::destroyVisibilityPropagationContextForPage(WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier pageID)
{
    RELEASE_LOG(Process, "ModelConnectionToWebProcess::destroyVisibilityPropagationContextForPage: pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, pageProxyID.toUInt64(), pageID.toUInt64());

    auto key = std::make_pair(pageProxyID, pageID);
    ASSERT(m_visibilityPropagationContexts.contains(key));
    m_visibilityPropagationContexts.remove(key);
}
#endif

void ModelConnectionToWebProcess::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
#if !RELEASE_LOG_DISABLED
    if  (auto* channel = WebCore::getLogChannel(channelName)) {
        channel->state = state;
        channel->level = level;
    }

    auto* channel = getLogChannel(channelName);
    if  (!channel)
        return;

    channel->state = state;
    channel->level = level;
#else
    UNUSED_PARAM(channelName);
    UNUSED_PARAM(state);
    UNUSED_PARAM(level);
#endif
}

bool ModelConnectionToWebProcess::allowsExitUnderMemoryPressure() const
{
    // FIXME: Should allow exit if we have no models.
    return false;
}

Logger& ModelConnectionToWebProcess::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        m_logger->setEnabled(this, m_sessionID.isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}

void ModelConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from WebContent process %" PRIu64 ".", description(messageName), m_webProcessIdentifier.toUInt64());
}

void ModelConnectionToWebProcess::lowMemoryHandler(Critical critical, Synchronous synchronous)
{
}

bool ModelConnectionToWebProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester.didReceiveMessage(connection, decoder);
        return true;
    }
#endif

    return messageReceiverMap().dispatchMessage(connection, decoder);
}

bool ModelConnectionToWebProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester.didReceiveSyncMessage(connection, decoder, replyEncoder);
        return true;
    }
#endif
    return messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(MODEL_PROCESS)
