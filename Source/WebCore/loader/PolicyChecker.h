/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/ContentFilterUnblockHandler.h>
#endif

namespace WTF {
template<typename> class CompletionHandler;
class CompletionHandlerCallingScope;
}

namespace WebCore {

class DocumentLoader;
class FormSubmission;
class HitTestResult;
class LocalFrame;
class NavigationAction;
class ResourceError;
class ResourceResponse;
class URLKeepingBlobAlive;

enum class NavigationPolicyDecision : uint8_t {
    ContinueLoad,
    IgnoreLoad,
    LoadWillContinueInAnotherProcess,
};

enum class NavigationNavigationType : uint8_t;

enum class PolicyDecisionMode { Synchronous, Asynchronous };

class PolicyChecker final : public CanMakeWeakPtr<PolicyChecker>, public CanMakeCheckedPtr<PolicyChecker> {
    WTF_MAKE_NONCOPYABLE(PolicyChecker);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(PolicyChecker, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PolicyChecker);
public:
    explicit PolicyChecker(LocalFrame&);

    using NavigationPolicyDecisionFunction = CompletionHandler<void(ResourceRequest&&, WeakPtr<const FormSubmission>&&, NavigationPolicyDecision)>;
    using NewWindowPolicyDecisionFunction = CompletionHandler<void(ResourceRequest&&, WeakPtr<const FormSubmission>&&, const AtomString& frameName, const NavigationAction&, ShouldContinuePolicyCheck)>;

    void checkNavigationPolicy(ResourceRequest&&, const ResourceResponse& redirectResponse, DocumentLoader*, RefPtr<const FormSubmission>&&, NavigationPolicyDecisionFunction&&, PolicyDecisionMode = PolicyDecisionMode::Asynchronous, std::optional<NavigationNavigationType> = std::nullopt);
    void checkNavigationPolicy(ResourceRequest&&, const ResourceResponse& redirectResponse, NavigationPolicyDecisionFunction&&);
    void checkNewWindowPolicy(NavigationAction&&, ResourceRequest&&, RefPtr<const FormSubmission>&&, const AtomString& frameName, NewWindowPolicyDecisionFunction&&);

    void stopCheck();

    void cannotShowMIMEType(const ResourceResponse&);

    FrameLoadType loadType() const { return m_loadType; }
    void setLoadType(FrameLoadType loadType) { m_loadType = loadType; }

    bool delegateIsDecidingNavigationPolicy() const { return m_delegateIsDecidingNavigationPolicy; }
    bool delegateIsHandlingUnimplementablePolicy() const { return m_delegateIsHandlingUnimplementablePolicy; }

#if ENABLE(CONTENT_FILTERING)
    void setContentFilterUnblockHandler(ContentFilterUnblockHandler&& unblockHandler) { m_contentFilterUnblockHandler = WTFMove(unblockHandler); }
#endif

private:
    void handleUnimplementablePolicy(const ResourceError&);
    URLKeepingBlobAlive extendBlobURLLifetimeIfNecessary(const ResourceRequest&, const Document&, PolicyDecisionMode = PolicyDecisionMode::Asynchronous) const;
    std::optional<HitTestResult> hitTestResult(const NavigationAction&);

    Ref<LocalFrame> protectedFrame() const;

    WeakRef<LocalFrame> m_frame;

    uint64_t m_javaScriptURLPolicyCheckIdentifier { 0 };
    bool m_delegateIsDecidingNavigationPolicy;
    bool m_delegateIsHandlingUnimplementablePolicy;

    // This identifies the type of navigation action which prompted this load. Note 
    // that WebKit conveys this value as the WebActionNavigationTypeKey value
    // on navigation action delegate callbacks.
    FrameLoadType m_loadType;

#if ENABLE(CONTENT_FILTERING)
    ContentFilterUnblockHandler m_contentFilterUnblockHandler;
#endif
};

} // namespace WebCore
