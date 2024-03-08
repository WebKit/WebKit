/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WKWebViewInternal.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionController.h"
#import "WebExtensionMatchPattern.h"
#import "_WKWebExtensionControllerInternal.h"
#import "_WKWebExtensionMatchPatternInternal.h"
#import <WebKit/_WKWebExtensionContext.h>
#import <WebKit/_WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/_WKWebExtensionMatchPattern.h>
#import <WebKit/_WKWebExtensionPermission.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>

namespace WebKit {

static NSString * const permissionsKey = @"permissions";
static NSString * const originsKey = @"origins";

void WebExtensionContext::permissionsGetAll(CompletionHandler<void(Vector<String>&&, Vector<String>&&)>&& completionHandler)
{
    Vector<String> permissions, origins;

    for (auto& permission : currentPermissions())
        permissions.append(permission);

    for (auto& matchPattern : currentPermissionMatchPatterns())
        origins.append(matchPattern->string());

    completionHandler(WTFMove(permissions), WTFMove(origins));
}

void WebExtensionContext::permissionsContains(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(hasPermissions(permissions, toPatterns(origins)));
}

void WebExtensionContext::permissionsRequest(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    auto matchPatterns = toPatterns(origins);

    // If there is nothing to grant, return true. This matches Chrome and Firefox.
    if (permissions.isEmpty() && origins.isEmpty()) {
        firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, permissions, matchPatterns);
        completionHandler(true);
        return;
    }

    if (hasPermissions(permissions, matchPatterns)) {
        completionHandler(true);
        return;
    }

    // There shouldn't be any unsupported permissions, since they got reported as an error before this.
    ASSERT(permissions.isSubset(extension().supportedPermissions()));

    bool requestedAllHostsPattern = WebExtensionMatchPattern::patternsMatchAllHosts(matchPatterns);
    if (requestedAllHostsPattern && !m_requestedOptionalAccessToAllHosts)
        m_requestedOptionalAccessToAllHosts = YES;

    class ResultHolder : public RefCounted<ResultHolder> {
    public:
        static Ref<ResultHolder> create() { return adoptRef(*new ResultHolder()); }
        ResultHolder() = default;

        bool matchPatternsAreGranted { false };
        bool permissionsAreGranted { false };
    };

    Ref resultHolder = ResultHolder::create();

    // Empty requests are automatically granted and don't call the delegate.
    resultHolder->matchPatternsAreGranted = matchPatterns.isEmpty();
    resultHolder->permissionsAreGranted = permissions.isEmpty();

    Ref callbackAggregator = CallbackAggregator::create([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), resultHolder, permissions, matchPatterns]() mutable {
        if (!resultHolder->matchPatternsAreGranted || !resultHolder->permissionsAreGranted) {
            completionHandler(false);
            return;
        }

        grantPermissionMatchPatterns(WTFMove(matchPatterns));
        grantPermissions(WTFMove(permissions));

        completionHandler(true);
    });

    auto matchPatternsCompletionHandler = [callbackAggregator, resultHolder, matchPatterns](NSSet<_WKWebExtensionMatchPattern *> *allowedMatchPatterns) {
        THROW_UNLESS([allowedMatchPatterns isKindOfClass:NSSet.class], @"Object returned by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: is not a set");

        for (_WKWebExtensionMatchPattern *pattern in allowedMatchPatterns) {
            THROW_UNLESS([pattern isKindOfClass:_WKWebExtensionMatchPattern.class], @"Object returned in set by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: is not a _WKWebExtensionMatchPattern");
            THROW_UNLESS(matchPatterns.contains(pattern._webExtensionMatchPattern), @"Set returned by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: doesn't contain the requested match patterns");
        }

        // The permissions.request() API only allows granting all or none.
        resultHolder->matchPatternsAreGranted = matchPatterns.size() == allowedMatchPatterns.count;
    };

    auto permissionsCompletionHandler = [callbackAggregator, resultHolder, permissions](NSSet<_WKWebExtensionPermission> *allowedPermissions) {
        THROW_UNLESS([allowedPermissions isKindOfClass:NSSet.class], @"Object returned by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: is not a set");

        for (_WKWebExtensionPermission permission in allowedPermissions) {
            THROW_UNLESS([permission isKindOfClass:NSString.class], @"Object returned in set by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: is not a _WKWebExtensionPermission");
            THROW_UNLESS(permissions.contains(permission), @"Result returned by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: doesn't contain the requested permissions");
        }

        // The permissions.request() API only allows granting all or none.
        resultHolder->permissionsAreGranted = permissions.size() == allowedPermissions.count;
    };

    auto delegate = extensionController()->delegate();

    if (!permissions.isEmpty() && [delegate respondsToSelector:@selector(webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler:)])
        [delegate webExtensionController:extensionController()->wrapper() promptForPermissions:toAPI(permissions) inTab:nil forExtensionContext:wrapper() completionHandler:makeBlockPtr(WTFMove(permissionsCompletionHandler)).get()];

    if (!matchPatterns.isEmpty() && [delegate respondsToSelector:@selector(webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler:)])
        [delegate webExtensionController:extensionController()->wrapper() promptForPermissionMatchPatterns:toAPI(matchPatterns) inTab:nil forExtensionContext:wrapper() completionHandler:makeBlockPtr(WTFMove(matchPatternsCompletionHandler)).get()];
}

void WebExtensionContext::permissionsRemove(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    auto matchPatterns = toPatterns(origins);
    bool removingAllHostsPattern = WebExtensionMatchPattern::patternsMatchAllHosts(matchPatterns);
    if (removingAllHostsPattern && m_requestedOptionalAccessToAllHosts)
        m_requestedOptionalAccessToAllHosts = false;

    removeGrantedPermissions(permissions);
    removeGrantedPermissionMatchPatterns(matchPatterns, EqualityOnly::No);

    completionHandler(!hasPermissions(permissions, matchPatterns));
}

void WebExtensionContext::firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType type, const PermissionsSet& permissions, const MatchPatternSet& matchPatterns)
{
    ASSERT(type == WebExtensionEventListenerType::PermissionsOnAdded || type == WebExtensionEventListenerType::PermissionsOnRemoved);

    HashSet<String> origins = toStrings(matchPatterns);

    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPermissionsEvent(type, permissions, origins));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
