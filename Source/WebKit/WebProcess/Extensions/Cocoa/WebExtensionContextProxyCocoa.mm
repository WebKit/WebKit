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

#include "config.h"
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionAPINamespace.h"
#include "WebExtensionAPIWebNavigation.h"
#include <WebCore/ProcessQualified.h>
#include <wtf/ObjectIdentifier.h>

namespace WebKit {

using namespace WebCore;

// MARK: webNavigation support

void WebExtensionContextProxy::dispatchWebNavigationOnBeforeNavigateEvent(WebPageProxyIdentifier pageID, WebCore::FrameIdentifier frameID, URL targetURL)
{
    NSDictionary *navigationDetails = @{
        @"url": [(NSURL *)targetURL absoluteString],

        // FIXME: We should be passing more arguments here and these arguments should have the correct values.
        @"tabId": @(pageID.toUInt64()),
        @"frameId": @(frameID.object().toUInt64())
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webNavigationObject = namespaceObject.webNavigation();
        webNavigationObject.onBeforeNavigate().invokeListenersWithArgument(navigationDetails, targetURL);
    });
}

void WebExtensionContextProxy::dispatchWebNavigationOnCommittedEvent(WebPageProxyIdentifier pageID, WebCore::FrameIdentifier frameID, URL frameURL)
{
    NSDictionary *navigationDetails = @{
        @"url": [(NSURL *)frameURL absoluteString],

        // FIXME: We should be passing more arguments here and these arguments should have the correct values.
        @"tabId": @(pageID.toUInt64()),
        @"frameId": @(frameID.object().toUInt64())
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webNavigationObject = namespaceObject.webNavigation();
        webNavigationObject.onCommitted().invokeListenersWithArgument(navigationDetails, frameURL);
    });
}

void WebExtensionContextProxy::dispatchWebNavigationOnDOMContentLoadedEvent(WebPageProxyIdentifier pageID, WebCore::FrameIdentifier frameID, URL frameURL)
{
    NSDictionary *navigationDetails = @{
        @"url": [(NSURL *)frameURL absoluteString],

        // FIXME: We should be passing more arguments here and these arguments should have the correct values.
        @"tabId": @(pageID.toUInt64()),
        @"frameId": @(frameID.object().toUInt64())
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webNavigationObject = namespaceObject.webNavigation();
        webNavigationObject.onDOMContentLoaded().invokeListenersWithArgument(navigationDetails, frameURL);
    });
}

void WebExtensionContextProxy::dispatchWebNavigationOnCompletedEvent(WebPageProxyIdentifier pageID, WebCore::FrameIdentifier frameID, URL frameURL)
{
    NSDictionary *navigationDetails = @{
        @"url": [(NSURL *)frameURL absoluteString],

        // FIXME: We should be passing more arguments here and these arguments should have the correct values.
        @"tabId": @(pageID.toUInt64()),
        @"frameId": @(frameID.object().toUInt64())
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webNavigationObject = namespaceObject.webNavigation();
        webNavigationObject.onCompleted().invokeListenersWithArgument(navigationDetails, frameURL);
    });
}

void WebExtensionContextProxy::dispatchWebNavigationOnErrorOccurredEvent(WebPageProxyIdentifier pageID, WebCore::FrameIdentifier frameID, URL frameURL)
{
    NSDictionary *navigationDetails = @{
        @"url": [(NSURL *)frameURL absoluteString],

        // FIXME: We should be passing more arguments here and these arguments should have the correct values.
        @"tabId": @(pageID.toUInt64()),
        @"frameId": @(frameID.object().toUInt64())
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webNavigationObject = namespaceObject.webNavigation();
        webNavigationObject.onErrorOccurred().invokeListenersWithArgument(navigationDetails, frameURL);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
