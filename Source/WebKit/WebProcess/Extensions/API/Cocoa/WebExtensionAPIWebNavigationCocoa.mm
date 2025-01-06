/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIWebNavigation.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionFrameIdentifier.h"
#import "WebExtensionFrameParameters.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static NSString *documentIdKey = @"documentId";
static NSString *errorOccurredKey = @"errorOccurred";
static NSString *frameIdKey = @"frameId";
static NSString *parentFrameIdKey = @"parentFrameId";
static NSString *tabIdKey = @"tabId";
static NSString *timeStampKey = @"timeStamp";
static NSString *urlKey = @"url";

static NSString * const emptyURLValue = @"";

static NSDictionary *toWebAPI(WebExtensionFrameParameters frameInfo)
{
    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    result[errorOccurredKey] = @(frameInfo.errorOccurred);
    result[parentFrameIdKey] = @(toWebAPI(frameInfo.parentFrameIdentifier));
    result[urlKey] = frameInfo.url && !frameInfo.url.value().isNull() ? (NSString *)frameInfo.url.value().string() : emptyURLValue;

    if (frameInfo.frameIdentifier)
        result[frameIdKey] = @(toWebAPI(frameInfo.frameIdentifier.value()));

    if (frameInfo.documentIdentifier)
        result[documentIdKey] = frameInfo.documentIdentifier.value().toString();

    return [result copy];
}

static NSArray<NSDictionary *> *toWebAPI(Vector<WebExtensionFrameParameters> allFrames)
{
    return createNSArray(allFrames, [](auto& frame) {
        return toWebAPI(frame);
    }).get();
}

void WebExtensionAPIWebNavigation::getAllFrames(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/getAllFrames

    static NSArray<NSString *> *requiredKeys = @[ tabIdKey ];
    static NSDictionary<NSString *, id> *types = @{
        tabIdKey: NSNumber.class,
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    NSNumber *tabId = details[tabIdKey];
    auto tabIdentifier = toWebExtensionTabIdentifier(tabId.doubleValue);
    if (!isValid(tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WebNavigationGetAllFrames(tabIdentifier.value()), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionFrameParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWebNavigation::getFrame(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/getFrame

    static NSArray<NSString *> *requiredKeys = @[ tabIdKey, frameIdKey ];
    static NSDictionary<NSString *, id> *types = @{
        tabIdKey: NSNumber.class,
        frameIdKey: NSNumber.class,
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    NSNumber *tabId = details[tabIdKey];
    auto tabIdentifier = toWebExtensionTabIdentifier(tabId.doubleValue);
    if (!isValid(tabIdentifier, outExceptionString))
        return;

    NSNumber *frameId = details[frameIdKey];
    auto frameIdentifier = toWebExtensionFrameIdentifier(frameId.doubleValue);
    if (!isValid(frameIdentifier)) {
        *outExceptionString = toErrorString(nil, frameIdKey, @"it is not a frame identifier");
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WebNavigationGetFrame(tabIdentifier.value(), frameIdentifier.value()), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionFrameParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onBeforeNavigate()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onBeforeNavigate

    if (!m_onBeforeNavigateEvent)
        m_onBeforeNavigateEvent = WebExtensionAPIWebNavigationEvent::create(*this, WebExtensionEventListenerType::WebNavigationOnBeforeNavigate);

    return *m_onBeforeNavigateEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onCommitted()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onCommitted

    if (!m_onCommittedEvent)
        m_onCommittedEvent = WebExtensionAPIWebNavigationEvent::create(*this, WebExtensionEventListenerType::WebNavigationOnCommitted);

    return *m_onCommittedEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onDOMContentLoaded()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onDOMContentLoaded

    if (!m_onDOMContentLoadedEvent)
        m_onDOMContentLoadedEvent = WebExtensionAPIWebNavigationEvent::create(*this, WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded);

    return *m_onDOMContentLoadedEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onCompleted()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onCompleted

    if (!m_onCompletedEvent)
        m_onCompletedEvent = WebExtensionAPIWebNavigationEvent::create(*this, WebExtensionEventListenerType::WebNavigationOnCompleted);

    return *m_onCompletedEvent;
}

WebExtensionAPIWebNavigationEvent& WebExtensionAPIWebNavigation::onErrorOccurred()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onErrorOccurred

    if (!m_onErrorOccurredEvent)
        m_onErrorOccurredEvent = WebExtensionAPIWebNavigationEvent::create(*this, WebExtensionEventListenerType::WebNavigationOnErrorOccurred);

    return *m_onErrorOccurredEvent;
}

void WebExtensionContextProxy::dispatchWebNavigationEvent(WebExtensionEventListenerType type, WebExtensionTabIdentifier tabID, const WebExtensionFrameParameters& frameParameters, WallTime timestamp)
{
    ASSERT(frameParameters.url);
    ASSERT(frameParameters.frameIdentifier);

    auto& frameURL = *frameParameters.url;
    auto& frameID = *frameParameters.frameIdentifier;
    auto& parentFrameID = frameParameters.parentFrameIdentifier;

    NSMutableDictionary *navigationDetails = [@{
        urlKey: frameURL.string(),
        tabIdKey: @(toWebAPI(tabID)),
        frameIdKey: @(toWebAPI(frameID)),
        parentFrameIdKey: @(toWebAPI(parentFrameID)),
        timeStampKey: @(floor(timestamp.approximateWallTime().secondsSinceEpoch().milliseconds()))
    } mutableCopy];

    if (frameParameters.documentIdentifier)
        navigationDetails[documentIdKey] = frameParameters.documentIdentifier.value().toString();

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webNavigationObject = namespaceObject.webNavigation();

        switch (type) {
        case WebExtensionEventListenerType::WebNavigationOnBeforeNavigate:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onBeforeNavigate
            webNavigationObject.onBeforeNavigate().invokeListenersWithArgument(navigationDetails, frameURL);
            break;

        case WebExtensionEventListenerType::WebNavigationOnCommitted:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onCommitted
            webNavigationObject.onCommitted().invokeListenersWithArgument(navigationDetails, frameURL);
            break;

        case WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onDOMContentLoaded
            webNavigationObject.onDOMContentLoaded().invokeListenersWithArgument(navigationDetails, frameURL);
            break;

        case WebExtensionEventListenerType::WebNavigationOnCompleted:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onCompleted
            webNavigationObject.onCompleted().invokeListenersWithArgument(navigationDetails, frameURL);
            break;

        case WebExtensionEventListenerType::WebNavigationOnErrorOccurred:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation/onErrorOccurred
            webNavigationObject.onErrorOccurred().invokeListenersWithArgument(navigationDetails, frameURL);
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
