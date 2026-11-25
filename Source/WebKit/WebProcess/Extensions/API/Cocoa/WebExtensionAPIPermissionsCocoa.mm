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
#import "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtension.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <WebCore/UserGestureIndicator.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {


void WebExtensionAPIPermissions::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/getAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsGetAll(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Vector<RefPtr<StringImpl>> permissions, Vector<RefPtr<StringImpl>> origins) {
        auto globalContext = callback->globalContext();

        auto permissionsValue = fromArray(globalContext, WTF::move(permissions));
        auto originsValue = fromArray(globalContext, WTF::move(origins));

        callback->call(fromObject(globalContext, {
            { StringImpl::create(permissionsKey), Protected(globalContext, permissionsValue) },
            { StringImpl::create(originsKey), Protected(globalContext, originsValue) }
        }));
    }, extensionContext().identifier());
}

void WebExtensionAPIPermissions::contains(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/contains

    static NSString * const apiName = @"permissions.contains()";

    HashSet<RefPtr<StringImpl>> permissions, origins;
    WebExtension::MatchPatternSet matchPatterns;
    if (!parseDetailsDictionary(details, permissions, origins, apiName, outExceptionString))
        return;

    if (!validatePermissionsDetails(permissions, origins, matchPatterns, apiName, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsContains(permissions, origins), [protectedThis = Ref { *this }, callback = WTF::move(callback)](bool containsPermissions) {
        callback->call(JSValueMakeBoolean(callback->globalContext(), containsPermissions));
    }, extensionContext().identifier());
}

void WebExtensionAPIPermissions::request(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/request

    static NSString * const apiName = @"permissions.request()";

    HashSet<RefPtr<StringImpl>> permissions, origins;
    if (!parseDetailsDictionary(details, permissions, origins, apiName, outExceptionString))
        return;

    NSString *errorMessage;
    WebExtension::MatchPatternSet matchPatterns;

    if (!validatePermissionsDetails(permissions, origins, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(StringImpl::create(apiName).get(), nullString(), StringImpl::create(errorMessage).get()).get());
        return;
    }

    if (!WebCore::UserGestureIndicator::processingUserGesture()) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(StringImpl::create(apiName).get(), nullString(), StringImpl::create(@"must be called during a user gesture").get()).get());
        return;
    }

    if (!verifyRequestedPermissions(permissions, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports these errors as callback errors and not exceptions, so do the same. Instead of round tripping
        // to the UI process, we can answer this here and report the error right away.
        callback->reportError(toErrorString(StringImpl::create(apiName).get(), nullString(), StringImpl::create(errorMessage).get()).get());
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsRequest(permissions, origins), [protectedThis = Ref { *this }, callback = WTF::move(callback)](bool success) {
        callback->call(JSValueMakeBoolean(callback->globalContext(), success));
    }, extensionContext().identifier());
}

void WebExtensionAPIPermissions::remove(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/remove

    static NSString * const apiName = @"permissions.remove()";

    HashSet<RefPtr<StringImpl>> permissions, origins;
    if (!parseDetailsDictionary(details, permissions, origins, apiName, outExceptionString))
        return;

    NSString *errorMessage;
    WebExtension::MatchPatternSet matchPatterns;
    if (!validatePermissionsDetails(permissions, origins, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(StringImpl::create(apiName).get(), nullString(), StringImpl::create(errorMessage).get()).get());
        return;
    }

    if (!verifyRequestedPermissions(permissions, matchPatterns, apiName, &errorMessage)) {
        // Chrome reports this error as callback error and not an exception, so do the same.
        callback->reportError(toErrorString(StringImpl::create(apiName).get(), nullString(), StringImpl::create(errorMessage).get()).get());
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::PermissionsRemove(permissions, origins), [protectedThis = Ref { *this }, callback = WTF::move(callback)](bool success) {
        callback->call(JSValueMakeBoolean(callback->globalContext(), success));
    }, extensionContext().identifier());
}

bool WebExtensionAPIPermissions::parseDetailsDictionary(NSDictionary *details, HashSet<RefPtr<StringImpl>>& permissions, HashSet<RefPtr<StringImpl>>& origins, NSString *callingAPIName, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        permissionsKey: @[ NSString.class ],
        originsKey: @[ NSString.class ],
    };

    if (!validateDictionary(details, permissionsKey, nil, types, outExceptionString))
        return false;

    for (NSString *permission in objectForKey<NSArray>(details, permissionsKey, true, NSString.class))
        permissions.add(StringImpl::create(permission));

    for (NSString *origin in objectForKey<NSArray>(details, originsKey, true, NSString.class))
        origins.add(StringImpl::create(origin));

    return true;
}

bool WebExtensionAPIPermissions::verifyRequestedPermissions(HashSet<RefPtr<StringImpl>>& permissions, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns, NSString *callingAPIName, NSString **outExceptionString)
{
    Ref extensionContext = this->extensionContext();
    RefPtr manifest = extensionContext->manifest();
    if (!manifest)
        return false;

    Ref extension = WebExtension::create(*manifest);
    HashSet<RefPtr<StringImpl>> allowedPermissions = extension->requestedPermissions();
    WebExtension::MatchPatternSet allowedHostPermissions = extension->allRequestedMatchPatterns();

    if ([callingAPIName isEqualToString:@"permissions.remove()"]) {
        if (permissions.size() && permissions.intersectionWith(allowedPermissions).size()) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(permissionsKey).get(), StringImpl::create(@"required permissions cannot be removed").get())).autorelease();
            return false;
        }

        for (auto& requestedPattern : matchPatterns) {
            for (auto& allowedPattern : allowedHostPermissions) {
                if (allowedPattern->matchesPattern(requestedPattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                    *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(originsKey).get(), StringImpl::create(@"required permissions cannot be removed").get())).autorelease();
                    return false;
                }
            }
        }
    }

    allowedPermissions.addAll(extension->optionalPermissions());
    allowedHostPermissions.addAll(extension->optionalPermissionMatchPatterns());

    bool requestingPermissionsNotDeclaredInManifest = (permissions.size() && !permissions.isSubset(allowedPermissions)) || (matchPatterns.size() && !allowedHostPermissions.size());
    if (requestingPermissionsNotDeclaredInManifest) {
        if ([callingAPIName isEqualToString:@"permissions.remove()"])
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(permissionsKey).get(), StringImpl::create(@"only permissions specified in the manifest may be removed").get())).autorelease();
        else
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(permissionsKey).get(), StringImpl::create(@"only permissions specified in the manifest may be requested").get())).autorelease();
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
                *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(originsKey).get(), StringImpl::create(@"only permissions specified in the manifest may be removed").get())).autorelease();
            else
                *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(originsKey).get(), StringImpl::create(@"only permissions specified in the manifest may be requested").get())).autorelease();
            return false;
        }
    }

    return true;
}

bool WebExtensionAPIPermissions::validatePermissionsDetails(HashSet<RefPtr<StringImpl>>& permissions, HashSet<RefPtr<StringImpl>>& origins, HashSet<Ref<WebExtensionMatchPattern>>& matchPatterns, NSString *callingAPIName, NSString **outExceptionString)
{
    for (auto& permission : permissions) {
        if (!WebExtension::supportedPermissions().contains(permission)) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(permissionsKey).get(), makeString("'"_s, permission, "' is not a valid permission"_s))).autorelease();
            return false;
        }
    }

    for (auto& origin : origins) {
        auto pattern = WebExtensionMatchPattern::getOrCreate(origin);
        if (!pattern || !pattern->isSupported()) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(originsKey).get(), makeString("'"_s, origin, "' is not a valid pattern"_s))).autorelease();
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
        lazyInitialize(m_onAdded, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::PermissionsOnAdded));

    return *m_onAdded;
}

WebExtensionAPIEvent& WebExtensionAPIPermissions::onRemoved()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onRemoved

    if (!m_onRemoved)
        lazyInitialize(m_onRemoved, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::PermissionsOnRemoved));

    return *m_onRemoved;
}

void WebExtensionContextProxy::dispatchPermissionsEvent(WebExtensionEventListenerType type, HashSet<RefPtr<StringImpl>> permissions, HashSet<RefPtr<StringImpl>> origins)
{
    auto *permissionDetails = toAPIArray(permissions);
    auto *originDetails = toAPIArray(origins);
    auto *details = @{ permissionsKey: permissionDetails, originsKey: originDetails };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        Ref permissionsObject = namespaceObject.permissions();

        switch (type) {
        case WebExtensionEventListenerType::PermissionsOnAdded:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onAdded
            protect(permissionsObject->onAdded())->invokeListenersWithArgument(details);
            break;

        case WebExtensionEventListenerType::PermissionsOnRemoved:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions/onRemoved
            protect(permissionsObject->onRemoved())->invokeListenersWithArgument(details);
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
