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
#import <WebKit/_WKWebExtensionContext.h>
#import <WebKit/_WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/_WKWebExtensionMatchPattern.h>
#import <WebKit/_WKWebExtensionPermission.h>

namespace WebKit {

static NSString * const permissionsKey = @"permissions";
static NSString * const originsKey = @"origins";

void WebExtensionContext::permissionsGetAll(CompletionHandler<void(Vector<String>, Vector<String>)>&& completionHandler)
{
    Vector<String> permissions, origins;

    for (auto& permission : currentPermissions())
        permissions.append(permission);

    for (auto& matchPattern : currentPermissionMatchPatterns())
        origins.append(matchPattern->string());

    completionHandler(permissions, origins);
}

void WebExtensionContext::permissionsContains(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    MatchPatternSet matchPatterns = toPatterns(origins);
    hasPermissions(permissions, matchPatterns) ? completionHandler(true) : completionHandler(false);
}

void WebExtensionContext::permissionsRequest(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    MatchPatternSet matchPatterns = toPatterns(origins);

    // If there is nothing to grant, return true. This matches Chrome and Firefox.
    if (!permissions.size() && !origins.size()) {
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

    __block MatchPatternSet grantedPatterns;
    __block PermissionsSet grantedPermissions = permissions;
    auto delegate = extensionController()->delegate();

    auto originsCompletionHandler = ^(NSSet<_WKWebExtensionMatchPattern *> *allowedOrigins) {
        // No granted origins were sent back, but origins were requested.
        if (!allowedOrigins.count && origins.size()) {
            completionHandler(false);
            return;
        }

        for (_WKWebExtensionMatchPattern *pattern in allowedOrigins) {
            // Allowed origin doesn't match the origins requested.
            if (!origins.contains(pattern.string)) {
                completionHandler(false);
                return;
            }

            grantedPatterns.add(*WebExtensionMatchPattern::getOrCreate(pattern.string));
        }

        grantPermissionMatchPatterns(WTFMove(grantedPatterns));
        grantPermissions(WTFMove(grantedPermissions));
        completionHandler(true);
    };

    auto permissionCompletionHandler = ^(NSSet<_WKWebExtensionPermission> *allowedPermissions) {
        // No granted permissions were sent back, but permissions were requested.
        if (!allowedPermissions.count && permissions.size()) {
            completionHandler(false);
            return;
        }

        for (_WKWebExtensionPermission permission in allowedPermissions) {
            // Allowed permission doesn't match the permissions requested.
            if (!grantedPermissions.contains(permission)) {
                completionHandler(false);
                return;
            }
        }

        if ([delegate respondsToSelector:@selector(webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler:)]) {
            NSSet *matchPatternsToRequest = toAPI(matchPatterns);
            [delegate webExtensionController:extensionController()->wrapper() promptForPermissionMatchPatterns:matchPatternsToRequest inTab:nil forExtensionContext:wrapper() completionHandler:originsCompletionHandler];
        } else
            completionHandler(false);
    };

    if ([delegate respondsToSelector:@selector(webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler:)]) {
        NSSet *permissionsToRequest = toAPI(permissions);
        [delegate webExtensionController:extensionController()->wrapper() promptForPermissions:permissionsToRequest inTab:nil forExtensionContext:wrapper() completionHandler:permissionCompletionHandler];
    } else
        completionHandler(false);
}

void WebExtensionContext::permissionsRemove(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    MatchPatternSet matchPatterns = toPatterns(origins);
    bool removingAllHostsPattern = WebExtensionMatchPattern::patternsMatchAllHosts(matchPatterns);
    if (removingAllHostsPattern && m_requestedOptionalAccessToAllHosts)
        m_requestedOptionalAccessToAllHosts = false;

    removeGrantedPermissions(permissions);
    removeGrantedPermissionMatchPatterns(matchPatterns, EqualityOnly::No);
    completionHandler(true);
}

void WebExtensionContext::firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType type, const PermissionsSet& permissions, const MatchPatternSet& matchPatterns)
{
    ASSERT(type == WebExtensionEventListenerType::PermissionsOnAdded || type == WebExtensionEventListenerType::PermissionsOnRemoved);

    HashSet<String> origins = toStrings(matchPatterns);

    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPermissionsEvent(type, permissions, origins));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
