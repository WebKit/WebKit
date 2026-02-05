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
#include "WebPageProxyIdentifier.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <optional>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebAutomationSession;
class WebFrameProxy;
class WebPageProxy;
struct FrameTreeNodeData;
struct FrameInfoData;

class BidiScriptAgent final : public Inspector::BidiScriptBackendDispatcherHandler, public CanMakeWeakPtr<BidiScriptAgent>, public CanMakeCheckedPtr<BidiScriptAgent> {
    WTF_MAKE_TZONE_ALLOCATED(BidiScriptAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BidiScriptAgent);

    struct FrameRealmCacheEntry {
        String url;
        std::optional<String> documentID;
        String realmId;
    };

public:
    BidiScriptAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiScriptAgent() override;

    // Inspector::BidiScriptBackendDispatcherHandler methods.
    void callFunction(const String& functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& optionalArguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&) override;
    void evaluate(const String& expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions,  std::optional<bool>&& optionalUserActivation, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&) override;
    void getRealms(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext& optionalBrowsingContext , std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&&) override;

private:
    void processRealmsForPagesAsync(Deque<Ref<WebPageProxy>>&& pagesToProcess, std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, std::optional<String>&& contextHandleFilter, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>&& accumulated, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&&);
    void collectExecutionReadyFrameRealms(const FrameTreeNodeData&, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>& realms, const std::optional<String>& contextHandleFilter, bool recurseSubframes = true);
    bool isFrameExecutionReady(const FrameInfoData&);
    RefPtr<Inspector::Protocol::BidiScript::RealmInfo> createRealmInfoForFrame(const FrameInfoData&);
    std::optional<String> contextHandleForFrame(const FrameInfoData&);
    String generateRealmIdForFrame(const FrameInfoData&);
    static String generateRealmIdForBrowsingContext(const String& browsingContext);
    static String originStringFromSecurityOriginData(const WebCore::SecurityOriginData&);

    void finishEvaluateBidiScriptResult(const String& realmId, const String& expression, Inspector::CommandResult<String>&&, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&&);

    // Stubbed realm registry to decouple evaluate from fabricated ids.
    class RealmRegistryStub {
    public:
        String realmIdForContext(const String& contextId) const;
        std::optional<String> contextForRealmId(const String& realmId) const;
    };

    RealmRegistryStub m_realmRegistry;
    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiScriptBackendDispatcher> m_scriptDomainDispatcher;

    // Track realm IDs to ensure they change when realms are recreated
    HashMap<WebCore::FrameIdentifier, FrameRealmCacheEntry> m_frameRealmCache; // frame ID -> (state signature, realm ID)

    // Track realm counters for navigation detection: frame ID -> counter
    HashMap<WebCore::FrameIdentifier, uint64_t> m_frameRealmCounters;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
