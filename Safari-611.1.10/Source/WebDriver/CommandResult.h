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

#pragma once

#include <wtf/Forward.h>
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

class CommandResult {
public:
    // §6.6 Handling Errors.
    // https://www.w3.org/TR/webdriver/#handling-errors
    enum class ErrorCode {
        ElementClickIntercepted,
        ElementNotSelectable,
        ElementNotInteractable,
        InvalidArgument,
        InvalidElementState,
        InvalidSelector,
        InvalidSessionID,
        JavascriptError,
        MoveTargetOutOfBounds,
        NoSuchAlert,
        NoSuchCookie,
        NoSuchElement,
        NoSuchFrame,
        NoSuchWindow,
        ScriptTimeout,
        SessionNotCreated,
        StaleElementReference,
        Timeout,
        UnableToCaptureScreen,
        UnexpectedAlertOpen,
        UnknownCommand,
        UnknownError,
        UnsupportedOperation,
    };

    static CommandResult success(RefPtr<JSON::Value>&& result = nullptr)
    {
        return CommandResult(WTFMove(result));
    }

    static CommandResult fail(RefPtr<JSON::Value>&& result = nullptr)
    {
        return CommandResult(WTFMove(result), CommandResult::ErrorCode::UnknownError);
    }

    static CommandResult fail(ErrorCode errorCode, Optional<String> errorMessage = WTF::nullopt)
    {
        return CommandResult(errorCode, errorMessage);
    }

    unsigned httpStatusCode() const;
    const RefPtr<JSON::Value>& result() const { return m_result; };
    void setAdditionalErrorData(RefPtr<JSON::Object>&& errorData) { m_errorAdditionalData = WTFMove(errorData); }
    bool isError() const { return !!m_errorCode; }
    ErrorCode errorCode() const { ASSERT(isError()); return m_errorCode.value(); }
    String errorString() const;
    Optional<String> errorMessage() const { ASSERT(isError()); return m_errorMessage; }
    const RefPtr<JSON::Object>& additionalErrorData() const { return m_errorAdditionalData; }

private:
    explicit CommandResult(RefPtr<JSON::Value>&&, Optional<ErrorCode> = WTF::nullopt);
    explicit CommandResult(ErrorCode, Optional<String> = WTF::nullopt);

    RefPtr<JSON::Value> m_result;
    Optional<ErrorCode> m_errorCode;
    Optional<String> m_errorMessage;
    RefPtr<JSON::Object> m_errorAdditionalData;
};

} // namespace WebDriver
