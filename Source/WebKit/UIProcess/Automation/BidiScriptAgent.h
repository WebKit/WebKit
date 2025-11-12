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

#pragma once

#if ENABLE(WEBDRIVER_BIDI)

#include "WebDriverBidiBackendDispatchers.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebAutomationSession;

class BidiScriptAgent final : public Inspector::BidiScriptBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiScriptAgent);
public:
    BidiScriptAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiScriptAgent() override;

    // Inspector::BidiScriptBackendDispatcherHandler methods.
    void callFunction(const String& functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& optionalArguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&) override;
    void evaluate(const String& expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions,  std::optional<bool>&& optionalUserActivation, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&) override;

private:
    struct ParsedStackTrace {
        Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::StackFrame>> callFrames;
        unsigned topLineNumber { 0 };
        unsigned topColumnNumber { 0 };
    };

    RefPtr<Inspector::Protocol::BidiScript::RemoteValue> serializeAsRemoteValue(RefPtr<JSON::Value>);

    void finishEvaluateBidiScriptResult(const String& realmId, const String& expression, Inspector::CommandResult<String>&&, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&);

    // Exception handling helpers
    ParsedStackTrace parseStackTrace(const String& stackTraceString);
    RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails> buildExceptionDetailsFromExceptionValue(RefPtr<JSON::Value> exceptionValue);

    // Stubbed realm registry to decouple evaluate from fabricated ids.
    class RealmRegistryStub {
    public:
        String realmIdForContext(const String& contextId) const;
        std::optional<String> contextForRealmId(const String& realmId) const;
    } m_realmRegistry;

    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiScriptBackendDispatcher> m_scriptDomainDispatcher;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
