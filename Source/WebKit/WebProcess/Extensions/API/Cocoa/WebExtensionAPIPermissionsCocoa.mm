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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtension.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

static NSString * const permissionsKey = @"permissions";
static NSString * const originsKey = @"origins";

void WebExtensionAPIPermissions::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/getAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsGetAll(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Vector<String> permissions, Vector<String> origins) {
        callback->call(@{
            permissionsKey: createNSArray(permissions).get(),
            originsKey: createNSArray(origins).get()
        });
    }, extensionContext().identifier());
}

void WebExtensionAPIPermissions::contains(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/contains

    static NSString * const apiName = @"permissions.contains()";

    HashSet<String> permissions, origins;
    WebExtension::MatchPatternSet matchPatterns;
    if (!parseDetailsDictionary(details, permissions, origins, apiName, outExceptionString))
        return;

    if (!validatePermissionsDetails(permissions, origins, matchPatterns, apiName, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsContains(permissions, origins), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool containsPermissions) {
        callback->call(containsPermissions ? @YES : @NO);
    }, extensionContext().identifier());
}

void WebExtensionAPIPermissions::request(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/request

    static NSString * const apiName = @"permissions.request()";

    HashSet<String> permissions, origins;
    if (!parseDetailsDictionary(details, permissions, origins, apiName, outExceptionString))
        return;

    NSString *errorMessage;
    WebExtension::MatchPatternSet matchPatterns;

    if (!validatePermissionsDetails(permissions, origins, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(apiName, nil, errorMessage));
        return;
    }

    if (!WebCore::UserGestureIndicator::processingUserGesture()) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(apiName, nil, @"must be called during a user gesture"));
        return;
    }

    if (!verifyRequestedPermissions(permissions, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports these errors as callback errors and not exceptions, so do the same. Instead of round tripping
        // to the UI process, we can answer this here and report the error right away.
        callback->reportError(toErrorString(apiName, nil, errorMessage));
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsRequest(permissions, origins), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool success) {
        callback->call(success ? @YES : @NO);
    }, extensionContext().identifier());
}

void WebExtensionAPIPermissions::remove(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/remove

    static NSString * const apiName = @"permissions.remove()";

    HashSet<String> permissions, origins;
    if (!parseDetailsDictionary(details, permissions, origins, apiName, outExceptionString))
        return;

    NSString *errorMessage;
    WebExtension::MatchPatternSet matchPatterns;
    if (!validatePermissionsDetails(permissions, origins, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(apiName, nil, errorMessage));
        return;
    }

    if (!verifyRequestedPermissions(permissions, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(apiName, nil, errorMessage));
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsRemove(permissions, origins), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool success) {
        callback->call(success ? @YES : @NO);
    }, extensionContext().identifier());
}

bool WebExtensionAPIPermissions::parseDetailsDictionary(NSDictionary *details, HashSet<String>& permissions, HashSet<String>& origins, NSString *callingAPIName, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        permissionsKey: @[ NSString.class ],
        originsKey: @[ NSString.class ],
    };

    if (!validateDictionary(details, permissionsKey, nil, types, outExceptionString))
        return false;

    for (NSString *permission in objectForKey<NSArray>(details, permissionsKey, true, NSString.class))
        permissions.add(permission);

    for (NSString *origin in objectForKey<NSArray>(details, originsKey, true, NSString.class))
        origins.add(origin);

    return true;
}

bool WebExtensionAPIPermissions::verifyRequestedPermissions(HashSet<String>& permissions, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns, NSString *callingAPIName, NSString **outExceptionString)
{
    auto extension = WebExtension::create(extensionContext().manifest());
    HashSet<String> allowedPermissions = extension->requestedPermissions();
    WebExtension::MatchPatternSet allowedHostPermissions = extension->allRequestedMatchPatterns();

    if ([callingAPIName isEqualToString:@"permissions.remove()"]) {
        if (bool requestingToRemoveFunctionalPermissions = permissions.size() && permissions.intersectionWith(allowedPermissions).size()) {
            *outExceptionString = toErrorString(nil, permissionsKey, @"required permissions cannot be removed");
            return false;
        }

        for (auto& requestedPattern : matchPatterns) {
            for (auto& allowedPattern : allowedHostPermissions) {
                if (allowedPattern->matchesPattern(requestedPattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                    *outExceptionString = toErrorString(nil, originsKey, @"required permissions cannot be removed");
                    return false;
                }
            }
        }
    }

    allowedPermissions.add(extension->optionalPermissions().begin(), extension->optionalPermissions().end());
    allowedHostPermissions.add(extension->optionalPermissionMatchPatterns().begin(), extension->optionalPermissionMatchPatterns().end());

    bool requestingPermissionsNotDeclaredInManifest = (permissions.size() && !permissions.isSubset(allowedPermissions)) || (matchPatterns.size() && !allowedHostPermissions.size());
    if (requestingPermissionsNotDeclaredInManifest) {
        if ([callingAPIName isEqualToString:@"permissions.remove()"])
            *outExceptionString = toErrorString(nil, permissionsKey, @"only permissions specified in the manifest may be removed");
        else
            *outExceptionString = toErrorString(nil, permissionsKey, @"only permissions specified in the manifest may be requested");
        return false;
    }

    for (auto& requestedPattern : matchPatterns) {
        bool matchFound = false;
        for (auto& allowedPattern : allowedHostPermissions) {
            if (allowedPattern->matchesPattern(requestedPattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                matchFound = true;
                break;
            }
        }

        if (!matchFound) {
            if ([callingAPIName isEqualToString:@"permissions.remove()"])
                *outExceptionString = toErrorString(nil, originsKey, @"only permissions specified in the manifest may be removed");
            else
                *outExceptionString = toErrorString(nil, originsKey, @"only permissions specified in the manifest may be requested");
            return false;
        }
    }

    return true;
}

bool WebExtensionAPIPermissions::validatePermissionsDetails(HashSet<String>& permissions, HashSet<String>& origins, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns, NSString *callingAPIName, NSString **outExceptionString)
{
    for (auto& permission : permissions) {
        if (!WebExtension::supportedPermissions().contains(permission)) {
            *outExceptionString = toErrorString(nil, permissionsKey, @"'%@' is not a valid permission", (NSString *)permission);
            return false;
        }
    }

    for (auto& origin : origins) {
        auto pattern = WebExtensionMatchPattern::getOrCreate(origin);
        if (!pattern || !pattern->isSupported()) {
            *outExceptionString = toErrorString(nil, originsKey, @"'%@' is not a valid pattern", (NSString *)origin);
            return false;
        }

        matchPatterns.add(*pattern);
    }

    return true;
}

WebExtensionAPIEvent& WebExtensionAPIPermissions::onAdded()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onAdded

    if (!m_onAdded)
        m_onAdded = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::PermissionsOnAdded);

    return *m_onAdded;
}

WebExtensionAPIEvent& WebExtensionAPIPermissions::onRemoved()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onRemoved

    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::PermissionsOnRemoved);

    return *m_onRemoved;
}

void WebExtensionContextProxy::dispatchPermissionsEvent(WebExtensionEventListenerType type, HashSet<String> permissions, HashSet<String> origins)
{
    auto *permissionDetails = toAPIArray(permissions);
    auto *originDetails = toAPIArray(origins);
    auto *details = @{ permissionsKey: permissionDetails, originsKey: originDetails };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& permissionsObject = namespaceObject.permissions();

        switch (type) {
        case WebExtensionEventListenerType::PermissionsOnAdded:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onAdded
            permissionsObject.onAdded().invokeListenersWithArgument(details);
            break;

        case WebExtensionEventListenerType::PermissionsOnRemoved:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onRemoved
            permissionsObject.onRemoved().invokeListenersWithArgument(details);
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
