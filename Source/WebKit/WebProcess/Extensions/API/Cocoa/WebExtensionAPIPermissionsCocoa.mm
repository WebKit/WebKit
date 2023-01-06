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
#import "WebExtensionAPIPermissions.h"

#import "WebExtension.h"
#import "WebExtensionContextMessages.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static NSString *permissionsKey = @"permissions";
static NSString *originsKey = @"origins";

void WebExtensionAPIPermissions::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    RELEASE_LOG(Extensions, "permissions.getAll()");

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsGetAll(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Vector<String> permissions, Vector<String> origins) {
        callback->call(@{
            permissionsKey: createNSArray(permissions).get(),
            originsKey: createNSArray(origins).get()
        });
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIPermissions::contains(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **)
{
    RELEASE_LOG(Extensions, "permissions.contains()");

    HashSet<String> permissions, origins;
    WebExtension::MatchPatternSet matchPatterns;
    parseDetailsDictionary(details, permissions, origins);

    NSString *errorMessage;
    if (!validatePermissionsDetails(permissions, origins, matchPatterns, @"contains()", &errorMessage)) {
        callback->reportError(errorMessage);
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsContains(permissions, origins), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool containsPermissions) {
        callback->call(containsPermissions ? @YES : @NO);
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIPermissions::request(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **)
{
    RELEASE_LOG(Extensions, "permissions.request()");

    HashSet<String> permissions, origins;
    parseDetailsDictionary(details, permissions, origins);

    NSString *errorMessage;
    WebExtension::MatchPatternSet matchPatterns;

    if (!validatePermissionsDetails(permissions, origins, matchPatterns, @"request()", &errorMessage)) {
        callback->reportError(errorMessage);
        return;
    }

    // FIXME: <https://webkit.org/b/250135> Check to see if the request is being made from a user gesture.

    if (!verifyRequestedPermissions(permissions, matchPatterns, @"request()", &errorMessage)) {
        callback->reportError(errorMessage);
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsRequest(permissions, origins), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool success) {
        callback->call(success ? @YES : @NO);
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIPermissions::remove(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **)
{
    RELEASE_LOG(Extensions, "permissions.remove()");

    HashSet<String> permissions, origins;
    parseDetailsDictionary(details, permissions, origins);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsRemove(permissions, origins), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool success) {
        callback->call(success ? @YES : @NO);
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIPermissions::parseDetailsDictionary(NSDictionary *details, HashSet<String>& permissions, HashSet<String>& origins)
{
    for (NSString *permission in details[permissionsKey])
        permissions.add(permission);

    for (NSString *origin in details[originsKey])
        origins.add(origin);
}

bool WebExtensionAPIPermissions::verifyRequestedPermissions(HashSet<String>& permissions, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns, NSString *callingAPIName, NSString **outExceptionString)
{
    WebExtension extension = WebExtension(extensionContext().manifest());
    HashSet<String> allowedPermissions = extension.requestedPermissions();

    if ([callingAPIName isEqualToString:@"remove()"]) {
        if (bool requestingToRemoveFunctionalPermissions = permissions.size() && permissions.isSubset(allowedPermissions)) {
            *outExceptionString = @"Required permissions cannot be removed.";
            return false;
        }
    }

    allowedPermissions.add(extension.optionalPermissions().begin(), extension.optionalPermissions().end());

    bool requestingPermissionsNotDefinedInManifest = permissions.size() && !permissions.isSubset(allowedPermissions);

    WebExtension::MatchPatternSet allowedPatterns = extension.requestedPermissionMatchPatterns();
    for (auto& requestedPattern : matchPatterns) {
        bool requestingAllowedOrigin = false;
        for (auto& allowedPattern : allowedPatterns) {
            if (requestedPattern->matchesPattern(allowedPattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                requestingAllowedOrigin = true;
                break;
            }
        }

        if (!requestingAllowedOrigin || requestingPermissionsNotDefinedInManifest) {
            *outExceptionString = [callingAPIName isEqualToString:@"remove()"] ? @"Only permissions specified in the manifest may be removed" : @"Only permissions specified in the manifest may be requested.";
            return false;
        }
    }

    return true;
}

bool WebExtensionAPIPermissions::validatePermissionsDetails(HashSet<String>& permissions, HashSet<String>& origins, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns, NSString *callingAPIName, NSString **outExceptionString)
{
    for (auto& permission : permissions) {
        if (!WebExtension::supportedPermissions().contains(permission)) {
            *outExceptionString = [NSString stringWithFormat:@"Invalid permission (%@) passed to %@.", static_cast<NSString *>(permission), callingAPIName];
            return false;
        }
    }

    NSError *error;
    for (auto& origin : origins) {
        auto pattern = WebExtensionMatchPattern::getOrCreate(static_cast<NSString *>(origin));
        if (!pattern || !pattern->isSupported()) {
            *outExceptionString = [NSString stringWithFormat:@"Invalid origin (%@) passed to %@. %@", static_cast<NSString *>(origin), callingAPIName, error];
            return false;
        }

        matchPatterns.add(*pattern);
    }

    return true;
}

WebExtensionAPIEvent& WebExtensionAPIPermissions::onAdded()
{
    if (!m_onAdded)
        m_onAdded = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext());

    return *m_onAdded;
}

WebExtensionAPIEvent& WebExtensionAPIPermissions::onRemoved()
{
    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext());

    return *m_onRemoved;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
