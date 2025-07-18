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

#if ENABLE(WEBDRIVER_BIDI)

#include "BidiBrowserAgent.h"
#include "BidiBrowsingContextAgent.h"
#include "BidiScriptAgent.h"
#include "BidiStorageAgent.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
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
    , m_browsingContextAgent(makeUniqueRef<BidiBrowsingContextAgent>(session, m_backendDispatcher))
    , m_scriptAgent(makeUniqueRef<BidiScriptAgent>(session, m_backendDispatcher))
    , m_storageAgent(makeUniqueRef<BidiStorageAgent>(session, m_backendDispatcher))
    , m_browsingContextDomainNotifier(makeUniqueRef<BidiBrowsingContextFrontendDispatcher>(m_frontendRouter))
    , m_logDomainNotifier(makeUniqueRef<BidiLogFrontendDispatcher>(m_frontendRouter))
{
    m_frontendRouter->connectFrontend(*this);
}

WebDriverBidiProcessor::~WebDriverBidiProcessor()
{
    m_frontendRouter->disconnectFrontend(*this);
}

void WebDriverBidiProcessor::processBidiMessage(const String& message)
{
    RefPtr session = m_session.get();
    if (!session) {
        LOG(Automation, "processBidiMessage of length %d not delivered, session is gone!", message.length());
        return;
    }

    LOG(Automation, "[s:%s] processBidiMessage of length %d", session->sessionIdentifier().utf8().data(), message.length());
    LOG(Automation, "%s", message.utf8().data());

    m_backendDispatcher->dispatch(message);
}

// Translate internal error messages that come from the inspector protocol payload.
// See the list of allowed bidi errors in https://w3c.github.io/webdriver-bidi/#errors
static String toBidiErrorCode(int errorCode, const String& inspectorInternalMsg)
{
    // These error codes are specified in JSON-RPC 2.0, Section 5.1.
    switch (errorCode) {
    case -32000: // Server error
        break;
    case -32700: // Parse error
    case -32600: // Invalid request
    case -32603: // Internal error
        return "unknown error"_s;
    case -32601: // Method not found
        return "unknown command"_s;
    case -32602: // Invalid params
        return "invalid argument"_s;
    }

    auto errorMessage = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::ErrorMessage>(inspectorInternalMsg);
    if (!errorMessage)
        return "unknown error"_s;

    switch (*errorMessage) {
    case Inspector::Protocol::Automation::ErrorMessage::InternalError:
        return "unknown error"_s;
    case Inspector::Protocol::Automation::ErrorMessage::Timeout:
        return "timeout"_s;
    case Inspector::Protocol::Automation::ErrorMessage::JavaScriptError:
        return "javascript error"_s;
    case Inspector::Protocol::Automation::ErrorMessage::JavaScriptTimeout:
        return "script timeout"_s;
    case Inspector::Protocol::Automation::ErrorMessage::WindowNotFound:
        return "no such window"_s;
    case Inspector::Protocol::Automation::ErrorMessage::FrameNotFound:
        return "no such frame"_s;
    case Inspector::Protocol::Automation::ErrorMessage::NodeNotFound:
        return "stale element reference"_s;
    case Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier:
        return "no such element"_s;
    case Inspector::Protocol::Automation::ErrorMessage::InvalidElementState:
        return "invalid element state"_s;
    case Inspector::Protocol::Automation::ErrorMessage::NoJavaScriptDialog:
        return "no such alert"_s;
    case Inspector::Protocol::Automation::ErrorMessage::NotImplemented:
        return "unsupported operation"_s;
    case Inspector::Protocol::Automation::ErrorMessage::MissingParameter:
    case Inspector::Protocol::Automation::ErrorMessage::InvalidParameter:
        return "invalid argument"_s;
    case Inspector::Protocol::Automation::ErrorMessage::InvalidSelector:
        return "invalid selector"_s;
    case Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable:
        return "element not interactable"_s;
    case Inspector::Protocol::Automation::ErrorMessage::ElementNotSelectable:
        return "element not selectable"_s;
    case Inspector::Protocol::Automation::ErrorMessage::ScreenshotError:
        return "unable to capture screen"_s;
    case Inspector::Protocol::Automation::ErrorMessage::UnexpectedAlertOpen:
        return "unexpected alert open"_s;
    case Inspector::Protocol::Automation::ErrorMessage::TargetOutOfBounds:
        return "move target out of bounds"_s;
    case Inspector::Protocol::Automation::ErrorMessage::UnableToLoadExtension:
        return "unable to load extension"_s;
    case Inspector::Protocol::Automation::ErrorMessage::UnableToUnloadExtension:
        return "unable to unload extension"_s;
    case Inspector::Protocol::Automation::ErrorMessage::NoSuchExtension:
        return "no such web extension"_s;
    default:
        return "unknown error"_s;
    }
}

void WebDriverBidiProcessor::sendBidiMessage(const String& message)
{
    RefPtr session = m_session.get();
    if (!session) {
        LOG(Automation, "sendBidiMessage of length %d not delivered, session is gone!", message.length());
        return;
    }

    LOG(Automation, "[s:%s] sendBidiMessage of length %d", session->sessionIdentifier().utf8().data(), message.length());
    LOG(Automation, "%s", message.utf8().data());

    auto msgValue = JSON::Object::parseJSON(message);
    if (!msgValue) {
        RELEASE_LOG_ERROR(Automation, "[s:%s] sendBidiMessage failed to parse message as JSON: %s", session->sessionIdentifier().utf8().data(), message.utf8().data());
        return;
    }
    auto msgObj = msgValue->asObject();
    if (!msgObj) {
        RELEASE_LOG_ERROR(Automation, "[s:%s] sendBidiMessage failed to parse message as JSON object: %s", session->sessionIdentifier().utf8().data(), message.utf8().data());
        return;
    }

    if (auto internalErrorObj = msgObj->getObject("error"_s)) {
        if (auto codeField = internalErrorObj->getInteger("code"_s)) {
            RELEASE_LOG(Automation, "[s:%s] sendBidiMessage converting internal error into BiDi error: %s", session->sessionIdentifier().utf8().data(), message.utf8().data());

            auto bidiErrorObj = JSON::Object::create();
            bidiErrorObj->setString("type"_s, "error"_s);
            auto internalMsg = internalErrorObj->getString("message"_s);
            bidiErrorObj->setString("message"_s, internalMsg);
            bidiErrorObj->setString("error"_s, toBidiErrorCode(*codeField, internalMsg));
            if (auto commandId = msgObj->getInteger("id"_s))
                bidiErrorObj->setInteger("id"_s, *commandId);

            session->sendBidiMessage(bidiErrorObj->toJSONString());
            return;
        }
        // FIXME should we forward some unknown error?
        RELEASE_LOG_ERROR(Automation, "[s:%s] sendBidiMessage failed to parse error code: %s", session->sessionIdentifier().utf8().data(), message.utf8().data());
    } else if (msgObj->getInteger("id"_s))
        msgObj->setString("type"_s, "success"_s);
    else
        msgObj->setString("type"_s, "event"_s);

    session->sendBidiMessage(msgObj->toJSONString());
}


// MARK: Inspector::FrontendChannel methods.

void WebDriverBidiProcessor::sendMessageToFrontend(const String& message)
{
    sendBidiMessage(message);
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
