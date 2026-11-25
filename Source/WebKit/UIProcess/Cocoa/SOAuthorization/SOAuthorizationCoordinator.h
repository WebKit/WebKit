/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#if HAVE(APP_SSO)

#include <wtf/BlockPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCountable.h>
#include <wtf/TZoneMalloc.h>

namespace API {
class NavigationAction;
class PageConfiguration;
}

namespace WebKit {

class WebPageProxy;

// The callers' callbacks, boxed so that the Swift session can keep them.
// FIXME: (rdar://170854276) Swift cannot spell a C++ class template instantiation, so these aliases name them.
using SOAuthorizationNavigationCallback = WTF::RefCountable<CompletionHandler<void(bool)>>;
using SOAuthorizationNewPageCallback = WTF::RefCountable<CompletionHandler<void(RefPtr<WebPageProxy>&&)>>;
using SOAuthorizationUIClientCallback = WTF::RefCountable<Function<void(Ref<API::NavigationAction>&&, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&&)>>;

// FIXME: (rdar://193738752) Swift cannot expose an actor-isolated class to C++, so C++ calls it through blocks.
// WKSOAuthorizationCoordinator's two tryAuthorize calls, as main-actor blocks that hold it.
// Spelled out: plain C++ files include this header, where NS_SWIFT_UI_ACTOR is not defined.
typedef void (__attribute__((swift_attr("@MainActor"))) ^SOAuthorizationTryAuthorizeNavigationBlock)(API::NavigationAction * _Nonnull, WebPageProxy * _Nonnull, SOAuthorizationNavigationCallback * _Nonnull);
typedef void (__attribute__((swift_attr("@MainActor"))) ^SOAuthorizationTryAuthorizePopUpBlock)(API::PageConfiguration * _Nonnull, API::NavigationAction * _Nonnull, WebPageProxy * _Nonnull, SOAuthorizationNewPageCallback * _Nonnull, SOAuthorizationUIClientCallback * _Nonnull);

struct SOAuthorizationCoordinatorBlocks {
    SOAuthorizationCoordinatorBlocks(SOAuthorizationTryAuthorizeNavigationBlock _Nonnull tryAuthorizeNavigation, SOAuthorizationTryAuthorizePopUpBlock _Nonnull tryAuthorizePopUp)
        : tryAuthorizeNavigation(makeBlockPtr(tryAuthorizeNavigation))
        , tryAuthorizePopUp(makeBlockPtr(tryAuthorizePopUp))
    {
    }

    BlockPtr<void(API::NavigationAction * _Nonnull, WebPageProxy * _Nonnull, SOAuthorizationNavigationCallback * _Nonnull)> tryAuthorizeNavigation;
    BlockPtr<void(API::PageConfiguration * _Nonnull, API::NavigationAction * _Nonnull, WebPageProxy * _Nonnull, SOAuthorizationNewPageCallback * _Nonnull, SOAuthorizationUIClientCallback * _Nonnull)> tryAuthorizePopUp;
};

class SOAuthorizationCoordinator {
    WTF_MAKE_TZONE_ALLOCATED(SOAuthorizationCoordinator);
    WTF_MAKE_NONCOPYABLE(SOAuthorizationCoordinator);
public:
    SOAuthorizationCoordinator();

    // For Navigation interception.
    void tryAuthorize(Ref<API::NavigationAction>&&, WebPageProxy&, Function<void(bool)>&&);

    // For PopUp interception.
    using NewPageCallback = CompletionHandler<void(RefPtr<WebPageProxy>&&)>;
    using UIClientCallback = Function<void(Ref<API::NavigationAction>&&, NewPageCallback&&)>;
    void tryAuthorize(Ref<API::PageConfiguration>&&, Ref<API::NavigationAction>&&, WebPageProxy&, NewPageCallback&&, UIClientCallback&&);

private:
    const SOAuthorizationCoordinatorBlocks m_blocks;
};

} // namespace WebKit

#endif
