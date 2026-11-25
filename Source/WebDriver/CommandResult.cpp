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

CommandResult::CommandResult(RefPtr<JSON::Value>&& result, std::optional<ErrorCode> errorCode)
    : m_errorCode(errorCode)
{
    if (!m_errorCode) {
        m_result = WTF::move(result);
        return;
    }

    if (!result)
        return;

    auto errorObject = result->asObject();
    if (!errorObject)
        return;

    auto errorMessage = errorObject->getString("message"_si);
    if (!errorMessage)
        return;

    auto error = errorObject->getInteger("code"_si);
    if (!error) {
        // Locally generated error bodies don't have the error code set,
        // which comes from the InspectorProtocol used to talk to the browser.
        if (m_errorCode == ErrorCode::UnknownError)
            m_errorMessage = errorMessage;
        return;
    }

    switch (*error) {
    case ProtocolErrorCode::ParseError:
    case ProtocolErrorCode::InvalidRequest:
    case ProtocolErrorCode::MethodNotFound:
    case ProtocolErrorCode::InvalidParams:
    case ProtocolErrorCode::InternalError:
        m_errorCode = ErrorCode::UnknownError;
        m_errorMessage = errorMessage;
        break;
    case ProtocolErrorCode::ServerError: {
        RefPtr<StringImpl> errorName;
        auto position = find(errorMessage, ';');
        if (position != notFound) {
            errorName = left(errorMessage, position);
            m_errorMessage = substring(errorMessage, position + 1);
        } else
            errorName = errorMessage;

        if (equal(errorName, "WindowNotFound"_s))
            m_errorCode = ErrorCode::NoSuchWindow;
        else if (equal(errorName, "FrameNotFound"_s))
            m_errorCode = ErrorCode::NoSuchFrame;
        else if (equal(errorName, "NotImplemented"_s))
            m_errorCode = ErrorCode::UnsupportedOperation;
        else if (equal(errorName, "ElementNotInteractable"_s))
            m_errorCode = ErrorCode::ElementNotInteractable;
        else if (equal(errorName, "JavaScriptError"_s))
            m_errorCode = ErrorCode::JavascriptError;
        else if (equal(errorName, "JavaScriptTimeout"_s))
            m_errorCode = ErrorCode::ScriptTimeout;
        else if (equal(errorName, "NodeNotFound"_s))
            m_errorCode = ErrorCode::NoSuchElement;
        else if (equal(errorName, "StaleNode"_s))
            m_errorCode = ErrorCode::StaleElementReference;
        else if (equal(errorName, "InvalidNodeIdentifier"_s))
            m_errorCode = ErrorCode::NoSuchElement;
        else if (equal(errorName, "MissingParameter"_s) || equal(errorName, "InvalidParameter"_s))
            m_errorCode = ErrorCode::InvalidArgument;
        else if (equal(errorName, "InvalidElementState"_s))
            m_errorCode = ErrorCode::InvalidElementState;
        else if (equal(errorName, "InvalidSelector"_s))
            m_errorCode = ErrorCode::InvalidSelector;
        else if (equal(errorName, "Timeout"_s))
            m_errorCode = ErrorCode::Timeout;
        else if (equal(errorName, "NoJavaScriptDialog"_s))
            m_errorCode = ErrorCode::NoSuchAlert;
        else if (equal(errorName, "ElementNotSelectable"_s))
            m_errorCode = ErrorCode::ElementNotSelectable;
        else if (equal(errorName, "ScreenshotError"_s))
            m_errorCode = ErrorCode::UnableToCaptureScreen;
        else if (equal(errorName, "UnexpectedAlertOpen"_s))
            m_errorCode = ErrorCode::UnexpectedAlertOpen;
        else if (equal(errorName, "TargetOutOfBounds"_s))
            m_errorCode = ErrorCode::MoveTargetOutOfBounds;

        break;
    }
    }
}

CommandResult::CommandResult(ErrorCode errorCode, std::optional<RefPtr<StringImpl>> errorMessage)
    : m_errorCode(errorCode)
    , m_errorMessage(errorMessage)
{
}

unsigned CommandResult::httpStatusCode() const
{
    if (!m_errorCode)
        return 200;

    return errorCodeToHTTPStatusCode(m_errorCode.value());
}

unsigned CommandResult::errorCodeToHTTPStatusCode(ErrorCode errorCode)
{
    // §6.6 Handling Errors.
    // https://www.w3.org/TR/webdriver/#handling-errors
    switch (errorCode) {
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
    case ErrorCode::NoSuchShadowRoot:
    case ErrorCode::StaleElementReference:
    case ErrorCode::DetachedShadowRoot:
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

RefPtr<StringImpl> CommandResult::errorString() const
{
    ASSERT(isError());
    return errorCodeToString(m_errorCode.value_or(ErrorCode::UnknownError));
}

RefPtr<StringImpl> CommandResult::errorCodeToString(ErrorCode errorCode)
{
    switch (errorCode) {
    case ErrorCode::ElementClickIntercepted:
        return "element click intercepted"_si;
    case ErrorCode::ElementNotSelectable:
        return "element not selectable"_si;
    case ErrorCode::ElementNotInteractable:
        return "element not interactable"_si;
    case ErrorCode::DetachedShadowRoot:
        return "detached shadow root"_si;
    case ErrorCode::InvalidArgument:
        return "invalid argument"_si;
    case ErrorCode::InvalidElementState:
        return "invalid element state"_si;
    case ErrorCode::InvalidSelector:
        return "invalid selector"_si;
    case ErrorCode::InvalidSessionID:
        return "invalid session id"_si;
    case ErrorCode::JavascriptError:
        return "javascript error"_si;
    case ErrorCode::NoSuchAlert:
        return "no such alert"_si;
    case ErrorCode::NoSuchCookie:
        return "no such cookie"_si;
    case ErrorCode::NoSuchElement:
        return "no such element"_si;
    case ErrorCode::NoSuchFrame:
        return "no such frame"_si;
    case ErrorCode::NoSuchShadowRoot:
        return "no such shadow root"_si;
    case ErrorCode::NoSuchWindow:
        return "no such window"_si;
    case ErrorCode::ScriptTimeout:
        return "script timeout"_si;
    case ErrorCode::SessionNotCreated:
        return "session not created"_si;
    case ErrorCode::StaleElementReference:
        return "stale element reference"_si;
    case ErrorCode::Timeout:
        return "timeout"_si;
    case ErrorCode::UnableToCaptureScreen:
        return "unable to capture screen"_si;
    case ErrorCode::MoveTargetOutOfBounds:
        return "move target out of bounds"_si;
    case ErrorCode::UnexpectedAlertOpen:
        return "unexpected alert open"_si;
    case ErrorCode::UnknownCommand:
        return "unknown command"_si;
    case ErrorCode::UnknownError:
        return "unknown error"_si;
    case ErrorCode::UnsupportedOperation:
        return "unsupported operation"_si;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

} // namespace WebDriver
