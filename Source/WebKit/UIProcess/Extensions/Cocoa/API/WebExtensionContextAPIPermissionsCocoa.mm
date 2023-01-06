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
#import "WebExtensionController.h"
#import "WebExtensionMatchPattern.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import "_WKWebExtensionControllerInternal.h"

namespace WebKit {

static NSString *permissionsKey = @"permissions";
static NSString *originsKey = @"origins";

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
    MatchPatternSet matchPatterns;
    parseMatchPatterns(origins, matchPatterns);

    hasPermissions(permissions, matchPatterns) ? completionHandler(true) : completionHandler(false);
}

void WebExtensionContext::permissionsRequest(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    HashSet<Ref<WebExtensionMatchPattern>> matchPatterns;
    parseMatchPatterns(origins, matchPatterns);

    if (hasPermissions(permissions, matchPatterns)) {
        completionHandler(true);
        return;
    }

    // FIXME: <https://webkit.org/b/250135> Call delegate method to prompt user for access.
}

void WebExtensionContext::permissionsRemove(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&& completionHandler)
{
    HashSet<Ref<WebExtensionMatchPattern>> matchPatterns;
    parseMatchPatterns(origins, matchPatterns);

    bool removingAllHostsPattern = false;
    for (auto& matchPattern : matchPatterns) {
        if (matchPattern->matchesAllHosts()) {
            removingAllHostsPattern = true;
            break;
        }
    }

    if (removingAllHostsPattern && m_requestedOptionalAccessToAllHosts)
        m_requestedOptionalAccessToAllHosts = false;

    removeGrantedPermissions(permissions);
    removeGrantedPermissionMatchPatterns(matchPatterns, EqualityOnly::No);
    completionHandler(true);
}

void WebExtensionContext::parseMatchPatterns(HashSet<String> origins, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns)
{
    for (auto& origin : origins) {
        auto pattern = WebExtensionMatchPattern::getOrCreate(static_cast<NSString *>(origin));
        matchPatterns.add(*pattern);
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
