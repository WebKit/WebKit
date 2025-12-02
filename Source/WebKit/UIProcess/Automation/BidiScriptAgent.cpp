/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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
#include "BidiScriptAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;
using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiScriptAgent);

BidiScriptAgent::BidiScriptAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_scriptDomainDispatcher(BidiScriptBackendDispatcher::create(backendDispatcher, this))
{
}

BidiScriptAgent::~BidiScriptAgent() = default;

void BidiScriptAgent::callFunction(const String& functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& arguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: handle non-BrowsingContext obtained from `Target`.
    std::optional<BrowsingContext> browsingContext = target->getString("context"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();

    // FIXME: handle `awaitPromise` option.
    // FIXME: handle `resultOwnership` option.
    // FIXME: handle `serializationOptions` option.
    // FIXME: handle custom `this` option.
    // FIXME: handle `userActivation` option.

    Ref<JSON::Array> argumentsArray = arguments ? arguments.releaseNonNull() : JSON::Array::create();

    session->evaluateJavaScriptFunction(topLevelContextHandle, frameHandle, functionDeclaration, WTFMove(argumentsArray), false, optionalUserActivation.value_or(false), std::nullopt, [callback = WTFMove(callback)](Inspector::CommandResult<String>&& stringResult) {
        // FIXME: Properly fill ExceptionDetails remaining fields once we have a way to get them instead of just the error message.
        // https://bugs.webkit.org/show_bug.cgi?id=288058
        if (!stringResult) {
            if (stringResult.error().startsWith("JavaScriptError"_s)) {
                auto exceptionValue = Inspector::Protocol::BidiScript::RemoteValue::create()
                    .setType(Inspector::Protocol::BidiScript::RemoteValueType::Error)
                    .release();
                auto stackTrace = Inspector::Protocol::BidiScript::StackTrace::create()
                    .setCallFrames(JSON::ArrayOf<Inspector::Protocol::BidiScript::StackFrame>::create())
                    .release();
                auto exceptionDetails = Inspector::Protocol::BidiScript::ExceptionDetails::create()
                    .setText(stringResult.error().right("JavaScriptError;"_s.length()))
                    .setLineNumber(0)
                    .setColumnNumber(0)
                    .setException(WTFMove(exceptionValue))
                    .setStackTrace(WTFMove(stackTrace))
                    .release();

                callback({ { Inspector::Protocol::BidiScript::EvaluateResultType::Exception, "placeholder_realm"_s, nullptr, WTFMove(exceptionDetails) } });
                return;
            }

            callback(makeUnexpected(stringResult.error()));
            return;
        }

        auto resultValue = JSON::Value::parseJSON(stringResult.value());
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!resultValue, InternalError, "Failed to parse callFunction result as JSON"_s);

        auto resultObject = Inspector::Protocol::BidiScript::RemoteValue::create()
            .setType(Inspector::Protocol::BidiScript::RemoteValueType::Object)
            .release();

        resultObject->setValue(resultValue.releaseNonNull());

        // FIXME: keep track of realm IDs that we hand out.
        callback({ { Inspector::Protocol::BidiScript::EvaluateResultType::Success, "placeholder_realm"_s, WTFMove(resultObject), nullptr } });
    });
}

void BidiScriptAgent::evaluate(const String& expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, std::optional<bool>&& optionalUserActivation, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: handle non-BrowsingContext obtained from `Target`.
    std::optional<BrowsingContext> browsingContext = target->getString("context"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();

    // FIXME: handle `awaitPromise` option.
    // FIXME: handle `resultOwnership` option.
    // FIXME: handle `serializationOptions` option.

    String functionDeclaration = makeString("function() {\n return "_s, expression, "; \n}"_s);
    session->evaluateJavaScriptFunction(topLevelContextHandle, frameHandle, functionDeclaration, JSON::Array::create(), false, optionalUserActivation.value_or(false), std::nullopt, [callback = WTFMove(callback)](Inspector::CommandResult<String>&& result) {
        auto evaluateResultType = result.has_value() ? Inspector::Protocol::BidiScript::EvaluateResultType::Success : Inspector::Protocol::BidiScript::EvaluateResultType::Exception;
        auto resultObject = Inspector::Protocol::BidiScript::RemoteValue::create()
            .setType(Inspector::Protocol::BidiScript::RemoteValueType::Object)
            .release();

        // FIXME: handle serializing different RemoteValue types as JSON here.
        if (result)
            resultObject->setValue(JSON::Value::create(WTFMove(result.value())));

        // FIXME: keep track of realm IDs that we hand out.
        callback({ { evaluateResultType, "placeholder_realm"_s, WTFMove(resultObject), nullptr } });
    });
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

