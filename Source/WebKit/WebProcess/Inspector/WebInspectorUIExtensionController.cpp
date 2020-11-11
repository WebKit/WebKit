/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WebInspectorUIExtensionController.h"

#if ENABLE(INSPECTOR_EXTENSIONS)

#include "WebInspectorUI.h"
#include "WebInspectorUIExtensionControllerMessages.h"
#include "WebInspectorUIExtensionControllerMessagesReplies.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/InspectorFrontendAPIDispatcher.h>

namespace WebKit {

WebInspectorUIExtensionController::WebInspectorUIExtensionController(WebCore::InspectorFrontendClient& inspectorFrontend)
    : m_frontendClient(makeWeakPtr(inspectorFrontend))
{
    Page* page = inspectorFrontend.frontendPage();
    ASSERT(page);

//    WTFReportBacktrace();
    WebProcess::singleton().addMessageReceiver(Messages::WebInspectorUIExtensionController::messageReceiverName(), WebPage::fromCorePage(*page).identifier(), *this);
}

WebInspectorUIExtensionController::~WebInspectorUIExtensionController()
{
//    WTFReportBacktrace();
    WebProcess::singleton().removeMessageReceiver(*this);
}

Optional<InspectorExtensionError> WebInspectorUIExtensionController::parseInspectorExtensionErrorFromResult(JSC::JSValue result)
{
    ASSERT(m_frontendClient);
    auto globalObject = m_frontendClient->frontendAPIDispatcher().frontendGlobalObject();
    if (!globalObject)
        return InspectorExtensionError::ContextDestroyed;

    // If the evaluation result is a string, the frontend returned an error string.
    // Anything else (falsy values, objects, arrays, DOM, etc.) is interpreted as success.
    if (result.isString()) {
        auto resultString = result.toWTFString(globalObject);
        if (resultString == "ContextDestroyed"_s)
            return InspectorExtensionError::ContextDestroyed;
        if (resultString == "InternalError"_s)
            return InspectorExtensionError::InternalError;
        if (resultString == "InvalidRequest"_s)
            return InspectorExtensionError::InvalidRequest;
        if (resultString == "RegistrationFailed"_s)
            return InspectorExtensionError::RegistrationFailed;

        ASSERT_NOT_REACHED();
        return InspectorExtensionError::InternalError;
    }

    return WTF::nullopt;
}

// WebInspectorUIExtensionController IPC messages.

void WebInspectorUIExtensionController::registerExtension(const String& extensionID, const String& displayName, CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&& completionHandler)
{
    if (!m_frontendClient) {
        completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
        return;
    }

    Vector<Ref<JSON::Value>> arguments {
        JSON::Value::create(extensionID),
        JSON::Value::create(displayName),
    };
    m_frontendClient->frontendAPIDispatcher().dispatchCommandWithResultAsync("registerExtension"_s, WTFMove(arguments), [weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)](InspectorFrontendAPIDispatcher::EvaluationResult&& result) mutable {
        if (!weakThis || !result) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        if (auto parsedError = weakThis->parseInspectorExtensionErrorFromResult(result.value())) {
            completionHandler(makeUnexpected(parsedError.value()));
            return;
        }

        completionHandler(true);
    });
}

void WebInspectorUIExtensionController::unregisterExtension(const String& extensionID, CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&& completionHandler)
{
    if (!m_frontendClient) {
        completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
        return;
    }

    Vector<Ref<JSON::Value>> arguments { JSON::Value::create(extensionID) };
    m_frontendClient->frontendAPIDispatcher().dispatchCommandWithResultAsync("unregisterExtension"_s, WTFMove(arguments), [weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)](InspectorFrontendAPIDispatcher::EvaluationResult&& result) mutable {
        if (!weakThis || !result) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        if (auto parsedError = weakThis->parseInspectorExtensionErrorFromResult(result.value())) {
            completionHandler(makeUnexpected(parsedError.value()));
            return;
        }

        completionHandler(true);
    });
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
