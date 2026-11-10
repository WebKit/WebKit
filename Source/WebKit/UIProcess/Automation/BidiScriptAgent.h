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

#include "IdentifierTypes.h"
#include "WebDriverBidiBackendDispatchers.h"
#include "WebPageProxyIdentifier.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <optional>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum PreloadScriptIdentifierType { };
using PreloadScriptIdentifier = ObjectIdentifier<PreloadScriptIdentifierType>;

class WebAutomationSession;
class WebFrameProxy;
class WebPageProxy;
struct FrameTreeNodeData;
struct FrameInfoData;

class BidiScriptAgent final : public Inspector::BidiScriptBackendDispatcherHandler, public CanMakeWeakPtr<BidiScriptAgent>, public CanMakeCheckedPtr<BidiScriptAgent> {
    WTF_MAKE_TZONE_ALLOCATED(BidiScriptAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BidiScriptAgent);

public:
    struct RealmInfo {
        WebCore::SecurityOriginData origin;
        Inspector::Protocol::BidiScript::RealmType type;
        Inspector::Protocol::BidiBrowsingContext::BrowsingContext context;
    };


    BidiScriptAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiScriptAgent() override;

    // Inspector::BidiScriptBackendDispatcherHandler methods.
    void addPreloadScript(StringImpl* functionDeclaration, RefPtr<JSON::Array>&& optionalArguments, RefPtr<JSON::Array>&& optionalContexts, StringImpl* optionalSandbox, RefPtr<JSON::Array>&& optionalUserContexts, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void removePreloadScript(StringImpl* script, Inspector::CommandCallback<void>&&) override;
    void callFunction(StringImpl* functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& optionalArguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, RefPtr<StringImpl>, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&) override;
    void disown(Ref<JSON::Array>&& handles, Ref<JSON::Object>&& target, Inspector::CommandCallback<void>&&) override;
    void evaluate(StringImpl* expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions,  std::optional<bool>&& optionalUserActivation, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, RefPtr<StringImpl>, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&) override;
    void getRealms(StringImpl* optionalBrowsingContext , std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&&) override;

    // Realm lifecycle events.
    void notifyRealmCreated(RealmIdentifier, Inspector::Protocol::BidiBrowsingContext::BrowsingContext, const WebCore::SecurityOriginData&);
    void notifyRealmDestroyed(RealmIdentifier, Inspector::Protocol::BidiBrowsingContext::BrowsingContext);

    // Lookup RealmIdentifier from browsing context (for UIProcess-initiated realm destruction).
    std::optional<RealmIdentifier> realmIdentifierForBrowsingContext(StringImpl* browsingContext) const;

    // Emit events for all currently active realms (called when subscribing to script.realmCreated).
    void emitEventsForActiveRealms(const HashSet<RefPtr<StringImpl>>& contextFilter = { });

    // Access active realms for realm destruction without WebFrameProxy.
    const HashMap<RealmIdentifier, RealmInfo>& activeRealms() const { return m_activeRealms; }

    void executePreloadScriptsForContext(StringImpl* browsingContext, StringImpl* frameHandle);

private:
    struct AllContextsTag { };

    struct PreloadScriptInfo {
        RefPtr<StringImpl> functionDeclaration;
        RefPtr<JSON::Array> arguments;
        Variant<AllContextsTag, Vector<RefPtr<StringImpl>>> contexts;
        RefPtr<StringImpl> sandbox;
        std::optional<Vector<RefPtr<StringImpl>>> userContexts;
    };

    void sendRealmCreatedEvent(StringImpl* realmID, const WebCore::SecurityOriginData&, Inspector::Protocol::BidiScript::RealmType, Inspector::Protocol::BidiBrowsingContext::BrowsingContext);

    void processRealmsForPagesAsync(Deque<Ref<WebPageProxy>>&& pagesToProcess, std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, std::optional<RefPtr<StringImpl>>&& contextHandleFilter, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>&& accumulated, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&&);
    void collectExecutionReadyFrameRealms(const FrameTreeNodeData&, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>& realms, const std::optional<RefPtr<StringImpl>>& contextHandleFilter, bool recurseSubframes = true);
    NODELETE bool isFrameExecutionReady(const FrameInfoData&);
    RefPtr<Inspector::Protocol::BidiScript::RealmInfo> createRealmInfoForFrame(const FrameInfoData&);
    std::optional<RefPtr<StringImpl>> contextHandleForFrame(const FrameInfoData&);
    RealmIdentifier generateRealmIdForFrame(const FrameInfoData&);
    RefPtr<StringImpl> generateRealmIdForBrowsingContext(StringImpl* browsingContext);
    static RefPtr<StringImpl> originStringFromSecurityOriginData(const WebCore::SecurityOriginData&);

    void finishEvaluateBidiScriptResult(StringImpl* realmID, StringImpl* expression, Inspector::CommandResult<RefPtr<StringImpl>>&&, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, RefPtr<StringImpl>, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&);

    static std::optional<RealmIdentifier> parseRealmIdentifier(StringImpl* realmID);
    std::optional<RefPtr<StringImpl>> browsingContextForRealm(RealmIdentifier) const;
    WeakPtr<WebAutomationSession> m_session;
    const Ref<Inspector::BidiScriptBackendDispatcher> m_scriptDomainDispatcher;

    // Track current realm identifier for each browsing context.
    HashMap<RefPtr<StringImpl>, RealmIdentifier> m_browsingContextToRealmId;

    // Store active realms (key: realm identifier, value: realm info).
    HashMap<RealmIdentifier, RealmInfo> m_activeRealms;

    // Track realm counters for navigation detection: frame ID -> counter

    Vector<std::pair<PreloadScriptIdentifier, PreloadScriptInfo>> m_preloadScripts;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
