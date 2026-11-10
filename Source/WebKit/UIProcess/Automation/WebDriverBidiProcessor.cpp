/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebDriverBidiProcessor.h"

#include "AutomationBackendDispatchers.h"
#include <optional>
#include <wtf/text/TextStream.h>

#if ENABLE(WEBDRIVER_BIDI)

#include "BidiBrowserAgent.h"
#include "BidiBrowsingContextAgent.h"
#include "BidiDigitalCredentialsAgent.h"
#include "BidiInputAgent.h"
#include "BidiPermissionsAgent.h"
#include "BidiScriptAgent.h"
#include "BidiSessionAgent.h"
#include "BidiStorageAgent.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/MathCommon.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebDriverBidiProcessor);

WebDriverBidiProcessor::WebDriverBidiProcessor(WebAutomationSession& session)
    : m_session(session)
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_browserAgent(makeUniqueRef<BidiBrowserAgent>(session, m_backendDispatcher))
    , m_browsingContextAgent(BidiBrowsingContextAgent::create(session, m_backendDispatcher))
    , m_digitalCredentialsAgent(makeUniqueRef<BidiDigitalCredentialsAgent>(session, m_backendDispatcher))
    , m_inputAgent(makeUniqueRef<BidiInputAgent>(session, m_backendDispatcher))
    , m_permissionsAgent(makeUniqueRef<BidiPermissionsAgent>(session, m_backendDispatcher))
    , m_scriptAgent(makeUniqueRef<BidiScriptAgent>(session, m_backendDispatcher))
    , m_sessionAgent(makeUniqueRef<BidiSessionAgent>(session, m_backendDispatcher))
    , m_storageAgent(makeUniqueRef<BidiStorageAgent>(session, m_backendDispatcher))
    , m_browsingContextDomainNotifier(makeUniqueRef<BidiBrowsingContextFrontendDispatcher>(m_frontendRouter))
    , m_inputDomainNotifier(makeUniqueRef<BidiInputFrontendDispatcher>(m_frontendRouter))
    , m_logDomainNotifier(makeUniqueRef<BidiLogFrontendDispatcher>(m_frontendRouter))
    , m_scriptDomainNotifier(makeUniqueRef<BidiScriptFrontendDispatcher>(m_frontendRouter))
{
    m_frontendRouter->connectFrontend(*this);
}

WebDriverBidiProcessor::~WebDriverBidiProcessor()
{
    m_frontendRouter->disconnectFrontend(*this);
}

void WebDriverBidiProcessor::processBidiMessage(StringImpl* message)
{
    RefPtr session = m_session.get();
    if (!session) {
        LOG(Automation, "processBidiMessage of length %d not delivered, session is gone!", length(message));
        return;
    }

    LOG_WITH_STREAM(Automation, stream << "[s:"_s << session->sessionIdentifier() << "] processBidiMessage of length "_s << length(message));
    LOG_WITH_STREAM(Automation, stream << message);

    m_backendDispatcher->dispatch(message);
}

Inspector::CommandResult<void> WebDriverBidiProcessor::validateSerializationOptions(const JSON::Object& serializationOptions)
{
    RefPtr<JSON::Value> maxDomDepthValue;
    if (serializationOptions.getValue("maxDomDepth"_si, maxDomDepthValue)) {
        if (auto maxDomDepth = maxDomDepthValue->asDouble()) {
            if (*maxDomDepth < 0)
                return makeUnexpected("serializationOptions.maxDomDepth must be non-negative"_s);
            if (std::floor(*maxDomDepth) != *maxDomDepth)
                return makeUnexpected("serializationOptions.maxDomDepth must be an integer"_s);
            if (*maxDomDepth > JSC::maxSafeInteger())
                return makeUnexpected("serializationOptions.maxDomDepth exceeds maximum safe integer"_s);
        }
    }

    RefPtr<JSON::Value> maxObjectDepthValue;
    if (serializationOptions.getValue("maxObjectDepth"_si, maxObjectDepthValue)) {
        if (auto maxObjectDepth = maxObjectDepthValue->asDouble()) {
            if (*maxObjectDepth < 0)
                return makeUnexpected("serializationOptions.maxObjectDepth must be non-negative"_s);
            if (std::floor(*maxObjectDepth) != *maxObjectDepth)
                return makeUnexpected("serializationOptions.maxObjectDepth must be an integer"_s);
            if (*maxObjectDepth > JSC::maxSafeInteger())
                return makeUnexpected("serializationOptions.maxObjectDepth exceeds maximum safe integer"_s);
        }
    }

    // Note: includeShadowTree validation is handled by the protocol enum definition in BidiScript.json

    return { };
}

// Translate internal error messages that come from the inspector protocol payload.
// See the list of allowed bidi errors in https://w3c.github.io/webdriver-bidi/#errors
static RefPtr<StringImpl> toBidiErrorCode(int errorCode, StringImpl* inspectorInternalMsg)
{
    // These error codes are specified in JSON-RPC 2.0, Section 5.1.
    switch (errorCode) {
    case -32000: // Server error
        break;
    case -32700: // Parse error
    case -32600: // Invalid request
    case -32603: // Internal error
        return "unknown error"_si;
    case -32601: // Method not found
        return "unknown command"_si;
    case -32602: // Invalid params
        return "invalid argument"_si;
    }

    auto errorMessage = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::ErrorMessage>(inspectorInternalMsg);
    if (!errorMessage)
        return "unknown error"_si;

    switch (*errorMessage) {
    case Inspector::Protocol::Automation::ErrorMessage::InternalError:
        return "unknown error"_si;
    case Inspector::Protocol::Automation::ErrorMessage::Timeout:
        return "timeout"_si;
    case Inspector::Protocol::Automation::ErrorMessage::JavaScriptError:
        return "javascript error"_si;
    case Inspector::Protocol::Automation::ErrorMessage::JavaScriptTimeout:
        return "script timeout"_si;
    case Inspector::Protocol::Automation::ErrorMessage::WindowNotFound:
        return "no such browsing context"_si;
    case Inspector::Protocol::Automation::ErrorMessage::FrameNotFound:
        return "no such frame"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NodeNotFound:
        return "no such node"_si;
    case Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier:
        return "no such element"_si;
    case Inspector::Protocol::Automation::ErrorMessage::InvalidElementState:
        return "invalid element state"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NoJavaScriptDialog:
        return "no such alert"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NotImplemented:
        return "unsupported operation"_si;
    case Inspector::Protocol::Automation::ErrorMessage::MissingParameter:
    case Inspector::Protocol::Automation::ErrorMessage::InvalidParameter:
        return "invalid argument"_si;
    case Inspector::Protocol::Automation::ErrorMessage::InvalidSelector:
        return "invalid selector"_si;
    case Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable:
        return "element not interactable"_si;
    case Inspector::Protocol::Automation::ErrorMessage::ElementNotSelectable:
        return "element not selectable"_si;
    case Inspector::Protocol::Automation::ErrorMessage::ScreenshotError:
        return "unable to capture screen"_si;
    case Inspector::Protocol::Automation::ErrorMessage::UnexpectedAlertOpen:
        return "unexpected alert open"_si;
    case Inspector::Protocol::Automation::ErrorMessage::TargetOutOfBounds:
        return "move target out of bounds"_si;
    case Inspector::Protocol::Automation::ErrorMessage::UnableToLoadExtension:
        return "unable to load extension"_si;
    case Inspector::Protocol::Automation::ErrorMessage::UnableToUnloadExtension:
        return "unable to unload extension"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NoSuchExtension:
        return "no such web extension"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NoSuchUserContext:
        return "no such user context"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NoSuchScript:
        return "no such script"_si;
    case Inspector::Protocol::Automation::ErrorMessage::NoSuchHistoryEntry:
        return "no such history entry"_si;
    default:
        return "unknown error"_si;
    }
}

void WebDriverBidiProcessor::sendBidiMessage(StringImpl* message)
{
    RefPtr session = m_session.get();
    if (!session) {
        LOG(Automation, "sendBidiMessage of length %d not delivered, session is gone!", length(message));
        return;
    }

    LOG_WITH_STREAM(Automation, stream << "[s:"_s << session->sessionIdentifier() << "] sendBidiMessage of length "_s << length(message));
    LOG_WITH_STREAM(Automation, stream << message);

    auto msgValue = JSON::Object::parseJSON(message);
    if (!msgValue) {
        RELEASE_LOG_ERROR(Automation, "[s:%s] sendBidiMessage failed to parse message as JSON: %s", utf8(session->sessionIdentifier()), utf8(message));
        return;
    }
    auto msgObj = msgValue->asObject();
    if (!msgObj) {
        RELEASE_LOG_ERROR(Automation, "[s:%s] sendBidiMessage failed to parse message as JSON object: %s", utf8(session->sessionIdentifier()), utf8(message));
        return;
    }

    if (auto internalErrorObj = msgObj->getObject("error"_si)) {
        if (auto codeField = internalErrorObj->getInteger("code"_si)) {
            RELEASE_LOG(Automation, "[s:%s] sendBidiMessage converting internal error into BiDi error: %s", utf8(session->sessionIdentifier()), utf8(message));

            auto bidiErrorObj = JSON::Object::create();
            bidiErrorObj->setString("type"_si, "error"_si);
            auto internalMsg = internalErrorObj->getString("message"_si);
            auto divot = find(internalMsg, ';');
            if (divot != notFound) {
                auto errorName = substring(internalMsg, 0, divot);
                auto errorDetail = substring(internalMsg, divot + 1);
                if (isEmpty(errorDetail))
                    errorDetail = "An error occurred."_si;
                bidiErrorObj->setString("message"_si, errorDetail);
                bidiErrorObj->setString("error"_si, toBidiErrorCode(*codeField, errorName));
            } else {
                bidiErrorObj->setString("message"_si, internalMsg);
                bidiErrorObj->setString("error"_si, toBidiErrorCode(*codeField, internalMsg));
            }
            if (auto commandId = msgObj->getInteger("id"_si))
                bidiErrorObj->setInteger("id"_si, *commandId);

            session->sendBidiMessage(bidiErrorObj->toJSONString());
            return;
        }
        // FIXME should we forward some unknown error?
        RELEASE_LOG_ERROR(Automation, "[s:%s] sendBidiMessage failed to parse error code: %s", utf8(session->sessionIdentifier()), utf8(message));
    } else if (msgObj->getInteger("id"_si))
        msgObj->setString("type"_si, "success"_si);
    else
        msgObj->setString("type"_si, "event"_si);

    session->sendBidiMessage(msgObj->toJSONString());
}

bool WebDriverBidiProcessor::eventIsEnabled(StringImpl* eventName, const HashSet<RefPtr<StringImpl>>& contexts)
{
    return m_sessionAgent->eventIsEnabled(eventName, contexts);
}

void WebDriverBidiProcessor::emitEventIfEnabled(StringImpl* eventName, const HashSet<RefPtr<StringImpl>>& browsingContexts, NOESCAPE const Function<void()>& callback)
{
    if (m_sessionAgent->eventIsEnabled(eventName, browsingContexts)) [[unlikely]]
        callback();
}

// MARK: Inspector::FrontendChannel methods.

void WebDriverBidiProcessor::sendMessageToFrontend(StringImpl* message)
{
    sendBidiMessage(message);
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
