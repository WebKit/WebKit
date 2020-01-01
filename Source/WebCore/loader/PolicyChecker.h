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

#include "FrameLoaderTypes.h"
#include "ResourceRequest.h"
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CONTENT_FILTERING)
#include "ContentFilterUnblockHandler.h"
#endif

namespace WTF {
template<typename> class CompletionHandler;
class CompletionHandlerCallingScope;
}

namespace WebCore {

class DocumentLoader;
class FormState;
class Frame;
class NavigationAction;
class ResourceError;
class ResourceResponse;

enum class NavigationPolicyDecision : uint8_t {
    ContinueLoad,
    IgnoreLoad,
    StopAllLoads,
};

enum class PolicyDecisionMode { Synchronous, Asynchronous };

class PolicyChecker {
    WTF_MAKE_NONCOPYABLE(PolicyChecker);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PolicyChecker(Frame&);

    enum class ShouldContinue {
        Yes,
        No
    };

    using NavigationPolicyDecisionFunction = CompletionHandler<void(ResourceRequest&&, WeakPtr<FormState>&&, NavigationPolicyDecision)>;
    using NewWindowPolicyDecisionFunction = CompletionHandler<void(const ResourceRequest&, WeakPtr<FormState>&&, const String& frameName, const NavigationAction&, ShouldContinue)>;

    void checkNavigationPolicy(ResourceRequest&&, const ResourceResponse& redirectResponse, DocumentLoader*, RefPtr<FormState>&&, NavigationPolicyDecisionFunction&&, PolicyDecisionMode = PolicyDecisionMode::Asynchronous);
    void checkNavigationPolicy(ResourceRequest&&, const ResourceResponse& redirectResponse, NavigationPolicyDecisionFunction&&);
    void checkNewWindowPolicy(NavigationAction&&, ResourceRequest&&, RefPtr<FormState>&&, const String& frameName, NewWindowPolicyDecisionFunction&&);

    void stopCheck();

    void cannotShowMIMEType(const ResourceResponse&);

    FrameLoadType loadType() const { return m_loadType; }
    void setLoadType(FrameLoadType loadType) { m_loadType = loadType; }

    bool delegateIsDecidingNavigationPolicy() const { return m_delegateIsDecidingNavigationPolicy; }
    bool delegateIsHandlingUnimplementablePolicy() const { return m_delegateIsHandlingUnimplementablePolicy; }

#if ENABLE(CONTENT_FILTERING)
    void setContentFilterUnblockHandler(ContentFilterUnblockHandler unblockHandler) { m_contentFilterUnblockHandler = WTFMove(unblockHandler); }
#endif

private:
    void handleUnimplementablePolicy(const ResourceError&);
    WTF::CompletionHandlerCallingScope extendBlobURLLifetimeIfNecessary(ResourceRequest&) const;

    Frame& m_frame;

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

// To support encoding WebCore::PolicyChecker::ShouldContinue in XPC messages
namespace WTF {

template<> struct EnumTraits<WebCore::PolicyChecker::ShouldContinue> {
    using values = EnumValues<
        WebCore::PolicyChecker::ShouldContinue,
        WebCore::PolicyChecker::ShouldContinue::No,
        WebCore::PolicyChecker::ShouldContinue::Yes
    >;
};

}
