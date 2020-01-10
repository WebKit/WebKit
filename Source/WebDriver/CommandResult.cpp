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

CommandResult::CommandResult(RefPtr<JSON::Value>&& result, Optional<ErrorCode> errorCode)
    : m_errorCode(errorCode)
{
    if (!m_errorCode) {
        m_result = WTFMove(result);
        return;
    }

    if (!result)
        return;

    RefPtr<JSON::Object> errorObject;
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
        else if (errorName == "InvalidNodeIdentifier")
            m_errorCode = ErrorCode::NoSuchElement;
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
        else if (errorName == "UnexpectedAlertOpen")
            m_errorCode = ErrorCode::UnexpectedAlertOpen;
        else if (errorName == "TargetOutOfBounds")
            m_errorCode = ErrorCode::MoveTargetOutOfBounds;

        break;
    }
    }
}

CommandResult::CommandResult(ErrorCode errorCode, Optional<String> errorMessage)
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
    case ErrorCode::JavascriptError:
    case ErrorCode::MoveTargetOutOfBounds:
    case ErrorCode::ScriptTimeout:
    case ErrorCode::SessionNotCreated:
    case ErrorCode::Timeout:
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
        return "element click intercepted"_s;
    case ErrorCode::ElementNotSelectable:
        return "element not selectable"_s;
    case ErrorCode::ElementNotInteractable:
        return "element not interactable"_s;
    case ErrorCode::InvalidArgument:
        return "invalid argument"_s;
    case ErrorCode::InvalidElementState:
        return "invalid element state"_s;
    case ErrorCode::InvalidSelector:
        return "invalid selector"_s;
    case ErrorCode::InvalidSessionID:
        return "invalid session id"_s;
    case ErrorCode::JavascriptError:
        return "javascript error"_s;
    case ErrorCode::NoSuchAlert:
        return "no such alert"_s;
    case ErrorCode::NoSuchCookie:
        return "no such cookie"_s;
    case ErrorCode::NoSuchElement:
        return "no such element"_s;
    case ErrorCode::NoSuchFrame:
        return "no such frame"_s;
    case ErrorCode::NoSuchWindow:
        return "no such window"_s;
    case ErrorCode::ScriptTimeout:
        return "script timeout"_s;
    case ErrorCode::SessionNotCreated:
        return "session not created"_s;
    case ErrorCode::StaleElementReference:
        return "stale element reference"_s;
    case ErrorCode::Timeout:
        return "timeout"_s;
    case ErrorCode::UnableToCaptureScreen:
        return "unable to capture screen"_s;
    case ErrorCode::MoveTargetOutOfBounds:
        return "move target out of bounds"_s;
    case ErrorCode::UnexpectedAlertOpen:
        return "unexpected alert open"_s;
    case ErrorCode::UnknownCommand:
        return "unknown command"_s;
    case ErrorCode::UnknownError:
        return "unknown error"_s;
    case ErrorCode::UnsupportedOperation:
        return "unsupported operation"_s;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

} // namespace WebDriver
