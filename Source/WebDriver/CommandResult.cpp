/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "CommandResult.h"

#include <inspector/InspectorValues.h>

using namespace Inspector;

namespace WebDriver {

// These error codes are specified in JSON-RPC 2.0, Section 5.1.
enum ProtocolErrorCode {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerError = -32000
};

CommandResult::CommandResult(RefPtr<InspectorValue>&& result, std::optional<ErrorCode> errorCode)
    : m_errorCode(errorCode)
{
    if (!m_errorCode) {
        m_result = WTFMove(result);
        return;
    }

    if (!result)
        return;

    RefPtr<InspectorObject> errorObject;
    if (!result->asObject(errorObject))
        return;

    int error;
    if (!errorObject->getInteger("code", error))
        return;
    String errorMessage;
    if (!errorObject->getString("message", errorMessage))
        return;

    switch (error) {
    case ProtocolErrorCode::ParseError:
    case ProtocolErrorCode::InvalidRequest:
    case ProtocolErrorCode::MethodNotFound:
    case ProtocolErrorCode::InvalidParams:
    case ProtocolErrorCode::InternalError:
        m_errorCode = ErrorCode::UnknownError;
        m_errorMessage = errorMessage;
        break;
    case ProtocolErrorCode::ServerError: {
        String errorName;
        auto position = errorMessage.find(';');
        if (position != notFound) {
            errorName = errorMessage.substring(0, position);
            m_errorMessage = errorMessage.substring(position + 1);
        } else
            errorName = errorMessage;

        if (errorName == "WindowNotFound")
            m_errorCode = ErrorCode::NoSuchWindow;
        else if (errorName == "FrameNotFound")
            m_errorCode = ErrorCode::NoSuchFrame;
        else if (errorName == "NotImplemented")
            m_errorCode = ErrorCode::UnsupportedOperation;
        else if (errorName == "ElementNotInteractable")
            m_errorCode = ErrorCode::ElementNotInteractable;
        else if (errorName == "JavaScriptError")
            m_errorCode = ErrorCode::JavascriptError;
        else if (errorName == "JavaScriptTimeout")
            m_errorCode = ErrorCode::ScriptTimeout;
        else if (errorName == "NodeNotFound")
            m_errorCode = ErrorCode::StaleElementReference;
        else if (errorName == "MissingParameter" || errorName == "InvalidParameter")
            m_errorCode = ErrorCode::InvalidArgument;
        else if (errorName == "InvalidElementState")
            m_errorCode = ErrorCode::InvalidElementState;
        else if (errorName == "InvalidSelector")
            m_errorCode = ErrorCode::InvalidSelector;
        else if (errorName == "Timeout")
            m_errorCode = ErrorCode::Timeout;
        else if (errorName == "NoJavaScriptDialog")
            m_errorCode = ErrorCode::NoSuchAlert;
        else if (errorName == "ElementNotSelectable")
            m_errorCode = ErrorCode::ElementNotSelectable;
        else if (errorName == "ScreenshotError")
            m_errorCode = ErrorCode::UnableToCaptureScreen;

        break;
    }
    }
}

CommandResult::CommandResult(ErrorCode errorCode, std::optional<String> errorMessage)
    : m_errorCode(errorCode)
    , m_errorMessage(errorMessage)
{
}

unsigned CommandResult::httpStatusCode() const
{
    if (!m_errorCode)
        return 200;

    // §6.6 Handling Errors.
    // https://www.w3.org/TR/webdriver/#handling-errors
    switch (m_errorCode.value()) {
    case ErrorCode::ElementClickIntercepted:
    case ErrorCode::ElementNotSelectable:
    case ErrorCode::ElementNotInteractable:
    case ErrorCode::InvalidArgument:
    case ErrorCode::InvalidElementState:
    case ErrorCode::InvalidSelector:
        return 400;
    case ErrorCode::NoSuchAlert:
    case ErrorCode::NoSuchCookie:
    case ErrorCode::NoSuchElement:
    case ErrorCode::NoSuchFrame:
    case ErrorCode::NoSuchWindow:
    case ErrorCode::StaleElementReference:
    case ErrorCode::InvalidSessionID:
    case ErrorCode::UnknownCommand:
        return 404;
    case ErrorCode::ScriptTimeout:
    case ErrorCode::Timeout:
        return 408;
    case ErrorCode::JavascriptError:
    case ErrorCode::SessionNotCreated:
    case ErrorCode::UnableToCaptureScreen:
    case ErrorCode::UnexpectedAlertOpen:
    case ErrorCode::UnknownError:
    case ErrorCode::UnsupportedOperation:
        return 500;
    }

    ASSERT_NOT_REACHED();
    return 200;
}

String CommandResult::errorString() const
{
    ASSERT(isError());

    switch (m_errorCode.value()) {
    case ErrorCode::ElementClickIntercepted:
        return ASCIILiteral("element click intercepted");
    case ErrorCode::ElementNotSelectable:
        return ASCIILiteral("element not selectable");
    case ErrorCode::ElementNotInteractable:
        return ASCIILiteral("element not interactable");
    case ErrorCode::InvalidArgument:
        return ASCIILiteral("invalid argument");
    case ErrorCode::InvalidElementState:
        return ASCIILiteral("invalid element state");
    case ErrorCode::InvalidSelector:
        return ASCIILiteral("invalid selector");
    case ErrorCode::InvalidSessionID:
        return ASCIILiteral("invalid session id");
    case ErrorCode::JavascriptError:
        return ASCIILiteral("javascript error");
    case ErrorCode::NoSuchAlert:
        return ASCIILiteral("no such alert");
    case ErrorCode::NoSuchCookie:
        return ASCIILiteral("no such cookie");
    case ErrorCode::NoSuchElement:
        return ASCIILiteral("no such element");
    case ErrorCode::NoSuchFrame:
        return ASCIILiteral("no such frame");
    case ErrorCode::NoSuchWindow:
        return ASCIILiteral("no such window");
    case ErrorCode::ScriptTimeout:
        return ASCIILiteral("script timeout");
    case ErrorCode::SessionNotCreated:
        return ASCIILiteral("session not created");
    case ErrorCode::StaleElementReference:
        return ASCIILiteral("stale element reference");
    case ErrorCode::Timeout:
        return ASCIILiteral("timeout");
    case ErrorCode::UnableToCaptureScreen:
        return ASCIILiteral("unable to capture screen");
    case ErrorCode::UnexpectedAlertOpen:
        return ASCIILiteral("unexpected alert open");
    case ErrorCode::UnknownCommand:
        return ASCIILiteral("unknown command");
    case ErrorCode::UnknownError:
        return ASCIILiteral("unknown error");
    case ErrorCode::UnsupportedOperation:
        return ASCIILiteral("unsupported operation");
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

} // namespace WebDriver
