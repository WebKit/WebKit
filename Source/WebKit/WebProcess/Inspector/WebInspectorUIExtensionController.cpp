/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "WebInspectorUI.h"
#include "WebInspectorUIExtensionControllerMessages.h"
#include "WebInspectorUIExtensionControllerMessagesReplies.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSCInlines.h>
#include <WebCore/ExceptionDetails.h>
#include <WebCore/InspectorFrontendAPIDispatcher.h>
#include <WebCore/SerializedScriptValue.h>

namespace WebKit {

WebInspectorUIExtensionController::WebInspectorUIExtensionController(WebCore::InspectorFrontendClient& inspectorFrontend)
    : m_frontendClient(makeWeakPtr(inspectorFrontend))
{
    auto* page = inspectorFrontend.frontendPage();
    ASSERT(page);

    WebProcess::singleton().addMessageReceiver(Messages::WebInspectorUIExtensionController::messageReceiverName(), WebPage::fromCorePage(*page).identifier(), *this);
}

WebInspectorUIExtensionController::~WebInspectorUIExtensionController()
{
    WebProcess::singleton().removeMessageReceiver(*this);
}

Optional<InspectorExtensionError> WebInspectorUIExtensionController::parseInspectorExtensionErrorFromEvaluationResult(InspectorFrontendAPIDispatcher::EvaluationResult result)
{
    if (!result) {
        switch (result.error()) {
        case WebCore::InspectorFrontendAPIDispatcher::EvaluationError::ContextDestroyed:
            return InspectorExtensionError::ContextDestroyed;
        case WebCore::InspectorFrontendAPIDispatcher::EvaluationError::ExecutionSuspended:
            return InspectorExtensionError::InternalError;
        case WebCore::InspectorFrontendAPIDispatcher::EvaluationError::InternalError:
            return InspectorExtensionError::InternalError;
        }
    }

    ASSERT(m_frontendClient);
    auto globalObject = m_frontendClient->frontendAPIDispatcher().frontendGlobalObject();
    if (!globalObject)
        return InspectorExtensionError::ContextDestroyed;

    auto valueOrException = result.value();
    if (!valueOrException.has_value()) {
        LOG(Inspector, "Encountered exception while evaluating upon the frontend: %s", valueOrException.error().message.utf8().data());
        return InspectorExtensionError::InternalError;
    }
    
    // If the evaluation result is a string, the frontend returned an error string.
    // If there was a JS exception, log it and return error code InternalError.
    // Anything else (falsy values, objects, arrays, DOM, etc.) is interpreted as success.
    JSC::JSValue scriptValue = valueOrException.value();
    if (scriptValue.isString()) {
        auto resultString = scriptValue.toWTFString(globalObject);
        if (resultString == "ContextDestroyed"_s)
            return InspectorExtensionError::ContextDestroyed;
        if (resultString == "InternalError"_s)
            return InspectorExtensionError::InternalError;
        if (resultString == "InvalidRequest"_s)
            return InspectorExtensionError::InvalidRequest;
        if (resultString == "RegistrationFailed"_s)
            return InspectorExtensionError::RegistrationFailed;
        if (resultString == "NotImplemented"_s)
            return InspectorExtensionError::NotImplemented;

        ASSERT_NOT_REACHED();
        return InspectorExtensionError::InternalError;
    }

    return WTF::nullopt;
}

// WebInspectorUIExtensionController IPC messages.

void WebInspectorUIExtensionController::registerExtension(const InspectorExtensionID& extensionID, const String& displayName, CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&& completionHandler)
{
    if (!m_frontendClient) {
        completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
        return;
    }

    Vector<Ref<JSON::Value>> arguments {
        JSON::Value::create(extensionID),
        JSON::Value::create(displayName),
    };
    m_frontendClient->frontendAPIDispatcher().dispatchCommandWithResultAsync("registerExtension"_s, WTFMove(arguments), [weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)](WebCore::InspectorFrontendAPIDispatcher::EvaluationResult&& result) mutable {
        if (!weakThis || !result) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        if (auto parsedError = weakThis->parseInspectorExtensionErrorFromEvaluationResult(result)) {
            completionHandler(makeUnexpected(parsedError.value()));
            return;
        }

        completionHandler(true);
    });
}

void WebInspectorUIExtensionController::unregisterExtension(const InspectorExtensionID& extensionID, CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&& completionHandler)
{
    if (!m_frontendClient) {
        completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
        return;
    }

    Vector<Ref<JSON::Value>> arguments { JSON::Value::create(extensionID) };
    m_frontendClient->frontendAPIDispatcher().dispatchCommandWithResultAsync("unregisterExtension"_s, WTFMove(arguments), [weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)](WebCore::InspectorFrontendAPIDispatcher::EvaluationResult&& result) mutable {
        if (!weakThis || !result) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        if (auto parsedError = weakThis->parseInspectorExtensionErrorFromEvaluationResult(result)) {
            completionHandler(makeUnexpected(parsedError.value()));
            return;
        }

        completionHandler(true);
    });
}

JSC::JSObject* WebInspectorUIExtensionController::unwrapEvaluationResultAsObject(InspectorFrontendAPIDispatcher::EvaluationResult result)
{
    if (!result)
        return nullptr;

    auto valueOrException = result.value();
    if (!valueOrException.has_value())
        return nullptr;

    return valueOrException.value().getObject();
}

void WebInspectorUIExtensionController::createTabForExtension(const InspectorExtensionID& extensionID, const String& tabName, const URL& tabIconURL, const URL& sourceURL, WTF::CompletionHandler<void(Expected<InspectorExtensionTabID, InspectorExtensionError>)>&& completionHandler)
{
    if (!m_frontendClient) {
        completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
        return;
    }

    Vector<Ref<JSON::Value>> arguments {
        JSON::Value::create(extensionID),
        JSON::Value::create(tabName),
        JSON::Value::create(tabIconURL.string()),
        JSON::Value::create(sourceURL.string()),
    };
    m_frontendClient->frontendAPIDispatcher().dispatchCommandWithResultAsync("createTabForExtension"_s, WTFMove(arguments), [weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)](InspectorFrontendAPIDispatcher::EvaluationResult&& result) mutable {
        if (!weakThis || !result) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        if (auto parsedError = weakThis->parseInspectorExtensionErrorFromEvaluationResult(result)) {
            completionHandler(makeUnexpected(parsedError.value()));
            return;
        }

        // Expected result is either an ErrorString or {extensionTabID: <string>}.
        auto objectResult = weakThis->unwrapEvaluationResultAsObject(result);
        if (!objectResult) {
            LOG(Inspector, "Unexpected non-object value returned from InspectorFrontendAPI.createTabForExtension().");
            completionHandler(makeUnexpected(InspectorExtensionError::InternalError));
            return;
        }

        auto* frontendGlobalObject = weakThis->m_frontendClient->frontendAPIDispatcher().frontendGlobalObject();
        JSC::JSValue foundProperty = objectResult->get(frontendGlobalObject, JSC::Identifier::fromString(frontendGlobalObject->vm(), "extensionTabID"_s));
        if (!foundProperty || !foundProperty.isString()) {
            completionHandler(makeUnexpected(InspectorExtensionError::InternalError));
            return;
        }

        completionHandler({ foundProperty.toWTFString(frontendGlobalObject) });
    });
}

void WebInspectorUIExtensionController::evaluateScriptForExtension(const InspectorExtensionID& extensionID, const String& scriptSource, const Optional<URL>& frameURL, const Optional<URL>& contextSecurityOrigin, const Optional<bool>& useContentScriptContext, CompletionHandler<void(const IPC::DataReference&, const Optional<WebCore::ExceptionDetails>&, const Optional<InspectorExtensionError>&)>&& completionHandler)
{
    if (!m_frontendClient) {
        completionHandler({ }, WTF::nullopt, InspectorExtensionError::InvalidRequest);
        return;
    }

    Ref<JSON::Object> optionalArguments = JSON::Object::create();
    if (frameURL)
        optionalArguments->setString("frameURL"_s, frameURL->string());
    if (contextSecurityOrigin)
        optionalArguments->setString("contextSecurityOrigin"_s, contextSecurityOrigin->string());
    if (useContentScriptContext)
        optionalArguments->setBoolean("useContentScriptContext"_s, *useContentScriptContext);

    Vector<Ref<JSON::Value>> arguments {
        JSON::Value::create(extensionID),
        JSON::Value::create(scriptSource),
        WTFMove(optionalArguments)
    };

    m_frontendClient->frontendAPIDispatcher().dispatchCommandWithResultAsync("evaluateScriptForExtension"_s, WTFMove(arguments), [weakThis = makeWeakPtr(this), completionHandler = WTFMove(completionHandler)](InspectorFrontendAPIDispatcher::EvaluationResult&& result) mutable {
        if (!weakThis) {
            completionHandler({ }, WTF::nullopt, InspectorExtensionError::ContextDestroyed);
            return;
        }

        auto* frontendGlobalObject = weakThis->m_frontendClient->frontendAPIDispatcher().frontendGlobalObject();
        if (!frontendGlobalObject) {
            completionHandler({ }, WTF::nullopt, InspectorExtensionError::ContextDestroyed);
            return;
        }

        if (auto parsedError = weakThis->parseInspectorExtensionErrorFromEvaluationResult(result)) {
            auto exceptionDetails = result.value().error();
            LOG(Inspector, "Internal error encountered while evaluating upon the frontend: at %s:%d:%d: %s", exceptionDetails.sourceURL.utf8().data(), exceptionDetails.lineNumber, exceptionDetails.columnNumber, exceptionDetails.message.utf8().data());
            completionHandler({ }, WTF::nullopt, parsedError);
            return;
        }

        // Expected result is either an ErrorString or {result: <any>}.
        auto objectResult = weakThis->unwrapEvaluationResultAsObject(result);
        if (!objectResult) {
            LOG(Inspector, "Unexpected non-object value returned from InspectorFrontendAPI.createTabForExtension().");
            completionHandler({ }, WTF::nullopt, InspectorExtensionError::InternalError);
            return;
        }
        ASSERT(result.value());

        JSC::JSValue errorPayload = objectResult->get(frontendGlobalObject, JSC::Identifier::fromString(frontendGlobalObject->vm(), "error"_s));
        if (!errorPayload.isUndefined()) {
            if (!errorPayload.isString()) {
                completionHandler({ }, WTF::nullopt, InspectorExtensionError::InternalError);
                return;
            }

            completionHandler({ }, ExceptionDetails { errorPayload.toWTFString(frontendGlobalObject) }, WTF::nullopt);
            return;
        }

        JSC::JSValue resultPayload = objectResult->get(frontendGlobalObject, JSC::Identifier::fromString(frontendGlobalObject->vm(), "result"_s));
        RefPtr<SerializedScriptValue> serializedResultValue = SerializedScriptValue::create(*frontendGlobalObject, resultPayload);
        if (!serializedResultValue) {
            completionHandler({ }, WTF::nullopt, InspectorExtensionError::InternalError);
            return;
        }

        completionHandler(serializedResultValue->data(), WTF::nullopt, WTF::nullopt);
    });
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
