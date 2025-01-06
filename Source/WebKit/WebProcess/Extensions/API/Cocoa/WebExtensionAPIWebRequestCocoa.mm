/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "ResourceLoadInfo.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIWebRequest.h"
#import <WebCore/HTTPStatusCodes.h>
#import <WebCore/ResourceResponse.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onBeforeRequest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeRequest

    if (!m_onBeforeRequestEvent)
        m_onBeforeRequestEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnBeforeRequest);

    return *m_onBeforeRequestEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onBeforeSendHeaders()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeSendHeaders

    if (!m_onBeforeSendHeadersEvent)
        m_onBeforeSendHeadersEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnBeforeSendHeaders);

    return *m_onBeforeSendHeadersEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onSendHeaders()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onSendHeaders

    if (!m_onSendHeadersEvent)
        m_onSendHeadersEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnSendHeaders);

    return *m_onSendHeadersEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onHeadersReceived()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onHeadersReceived

    if (!m_onHeadersReceivedEvent)
        m_onHeadersReceivedEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnHeadersReceived);

    return *m_onHeadersReceivedEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onAuthRequired()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onAuthRequired

    if (!m_onAuthRequiredEvent)
        m_onAuthRequiredEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnAuthRequired);

    return *m_onAuthRequiredEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onBeforeRedirect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onBeforeRedirect

    if (!m_onBeforeRedirectEvent)
        m_onBeforeRedirectEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnBeforeRedirect);

    return *m_onBeforeRedirectEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onResponseStarted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onResponseStarted

    if (!m_onResponseStartedEvent)
        m_onResponseStartedEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnResponseStarted);

    return *m_onResponseStartedEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onCompleted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onCompleted

    if (!m_onCompletedEvent)
        m_onCompletedEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnCompleted);

    return *m_onCompletedEvent;
}

WebExtensionAPIWebRequestEvent& WebExtensionAPIWebRequest::onErrorOccurred()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onErrorOccurred

    if (!m_onErrorOccurredEvent)
        m_onErrorOccurredEvent = WebExtensionAPIWebRequestEvent::create(*this, WebExtensionEventListenerType::WebRequestOnErrorOccurred);

    return *m_onErrorOccurredEvent;
}

static NSDictionary *convertRequestBodyToWebExtensionFormat(NSData *requestBody)
{
    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];

    NSURLComponents *urlComponents = [NSURLComponents componentsWithString:[NSString stringWithFormat:@"https://example.org/?%@", [[NSString alloc] initWithData:requestBody encoding:NSUTF8StringEncoding]]];
    for (NSURLQueryItem *item in urlComponents.queryItems) {
        NSMutableArray *array = dictionary[item.name];
        if (!array) {
            array = [NSMutableArray array];
            dictionary[item.name] = array;
        }
        [array addObject:item.value];
    }

    return dictionary;
}

static NSString *toWebAPI(ResourceLoadInfo::Type type)
{
    switch (type) {
    case ResourceLoadInfo::Type::ApplicationManifest:
        return @"web_manifest";
    case ResourceLoadInfo::Type::Beacon:
        return @"beacon";
    case ResourceLoadInfo::Type::CSPReport:
        return @"csp_report";
    case ResourceLoadInfo::Type::Document:
        return @"main_frame";
    case ResourceLoadInfo::Type::Font:
        return @"font";
    case ResourceLoadInfo::Type::Image:
        return @"image";
    case ResourceLoadInfo::Type::Media:
        return @"media";
    case ResourceLoadInfo::Type::Object:
        return @"object";
    case ResourceLoadInfo::Type::Ping:
        return @"ping";
    case ResourceLoadInfo::Type::Script:
        return @"script";
    case ResourceLoadInfo::Type::Stylesheet:
        return @"stylesheet";
    case ResourceLoadInfo::Type::Fetch:
    case ResourceLoadInfo::Type::XMLHTTPRequest:
        return @"xmlhttprequest";
    case ResourceLoadInfo::Type::XSLT:
        return @"xslt";
    case ResourceLoadInfo::Type::Other:
    default:
        return @"other";
    }
}

static NSMutableDictionary *webRequestDetailsForResourceLoad(const ResourceLoadInfo& resourceLoad, WebExtensionTabIdentifier tabIdentifier)
{
    NSMutableDictionary *result = [@{
        @"frameId": resourceLoad.parentFrameID ? @(toWebAPI(toWebExtensionFrameIdentifier(resourceLoad.frameID))) : @(toWebAPI(WebExtensionFrameConstants::MainFrameIdentifier)),
        @"parentFrameId": resourceLoad.parentFrameID ? @(toWebAPI(toWebExtensionFrameIdentifier(resourceLoad.parentFrameID))) : @(toWebAPI(WebExtensionFrameConstants::NoneIdentifier)),
        @"requestId": [NSString stringWithFormat:@"%llu", resourceLoad.resourceLoadID.toUInt64()],
        @"timeStamp": @(floor(resourceLoad.eventTimestamp.approximateWallTime().secondsSinceEpoch().milliseconds())),
        @"url": resourceLoad.originalURL.string(),
        @"tabId": @(toWebAPI(tabIdentifier)),
        @"type": toWebAPI(resourceLoad.type),
        @"method": resourceLoad.originalHTTPMethod,
    } mutableCopy];

    if (resourceLoad.documentID)
        result[@"documentId"] = resourceLoad.documentID.value().toString();

    return result;
}

static NSArray *convertHeaderFieldsToWebExtensionFormat(const WebCore::HTTPHeaderMap& headerMap)
{
    auto *convertedHeaderFields = [NSMutableArray arrayWithCapacity:headerMap.size()];
    for (auto header : headerMap) {
        [convertedHeaderFields addObject:@{
            @"name": header.key,
            @"value": header.value
        }];
    }

    // FIXME: <rdar://problem/58967376> Add cookies.

    return [convertedHeaderFields copy];
}

static NSMutableDictionary *headersReceivedDetails(const ResourceLoadInfo& resourceLoad, WebExtensionTabIdentifier tabID, const WebCore::ResourceResponse& response)
{
    auto *details = webRequestDetailsForResourceLoad(resourceLoad, tabID);

    if (auto *httpResponse = dynamic_objc_cast<NSHTTPURLResponse>(response.nsURLResponse())) {
        [details addEntriesFromDictionary:@{
            @"statusLine": bridge_id_cast(CFHTTPMessageCopyResponseStatusLine(CFURLResponseGetHTTPResponse(httpResponse._CFURLResponse))),
            // FIXME: <rdar://problem/59922101> responseHeaders (here and elsewhere) and all other optional members should check the options object.
            @"responseHeaders": convertHeaderFieldsToWebExtensionFormat(response.httpHeaderFields()),
            @"statusCode": @(httpResponse.statusCode),
            @"fromCache": @(resourceLoad.loadedFromCache)
            // FIXME: <rdar://problem/57132290> Add ip.
        }];
    }

    return details;
}

void WebExtensionContextProxy::resourceLoadDidSendRequest(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceRequest& request, const ResourceLoadInfo& resourceLoad)
{
    auto *details = webRequestDetailsForResourceLoad(resourceLoad, tabID);

    // FIXME: <rdar://problem/59922101> Chrome documentation says this about requestBody:
    // Only provided if extraInfoSpec contains 'requestBody'.
    if (auto *requestBody = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).HTTPBody)
        details[@"requestBody"] = convertRequestBodyToWebExtensionFormat(requestBody);

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webRequestObject = namespaceObject.webRequest();
        webRequestObject.onBeforeRequest().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });

    [details removeObjectForKey:@"requestBody"];
    details[@"requestHeaders"] = convertHeaderFieldsToWebExtensionFormat(request.httpHeaderFields());

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webRequestObject = namespaceObject.webRequest();
        webRequestObject.onBeforeSendHeaders().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
        webRequestObject.onSendHeaders().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });
}

void WebExtensionContextProxy::resourceLoadDidPerformHTTPRedirection(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceResponse& response, const ResourceLoadInfo& resourceLoad, const WebCore::ResourceRequest& newRequest)
{
    auto *details = headersReceivedDetails(resourceLoad, tabID, response);

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webRequestObject = namespaceObject.webRequest();
        webRequestObject.onHeadersReceived().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });

    details[@"redirectUrl"] = newRequest.url().string();

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webRequestObject = namespaceObject.webRequest();
        webRequestObject.onBeforeRedirect().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });
}

void WebExtensionContextProxy::resourceLoadDidReceiveChallenge(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::AuthenticationChallenge& webCoreChallenge, const ResourceLoadInfo& resourceLoad)
{
    auto *challenge = webCoreChallenge.nsURLAuthenticationChallenge();
    auto *httpResponse = dynamic_objc_cast<NSHTTPURLResponse>(challenge.failureResponse);
    if (!httpResponse)
        return;

    // Firefox only calls onAuthRequired when the status code is 401 or 407.
    // See https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/onAuthRequired
    if (httpResponse.statusCode != httpStatus401Unauthorized && httpResponse.statusCode != httpStatus407ProxyAuthenticationRequired)
        return;

    auto *details = headersReceivedDetails(resourceLoad, tabID, webCoreChallenge.failureResponse());
    [details addEntriesFromDictionary:@{
        @"scheme": [challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodHTTPDigest] ? @"digest" : @"basic",
        @"challenger": @{
            @"host": challenge.protectionSpace.host,
            @"port": @(challenge.protectionSpace.port)
        },
        @"isProxy": @(challenge.protectionSpace.isProxy),
    }];

    if (auto *realm = challenge.protectionSpace.realm)
        details[@"realm"] = realm;

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webRequestObject = namespaceObject.webRequest();
        webRequestObject.onAuthRequired().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });
}

void WebExtensionContextProxy::resourceLoadDidReceiveResponse(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceResponse& response, const ResourceLoadInfo& resourceLoad)
{
    auto *details = headersReceivedDetails(resourceLoad, tabID, response);

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& webRequestObject = namespaceObject.webRequest();
        webRequestObject.onHeadersReceived().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
        webRequestObject.onResponseStarted().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });
}

void WebExtensionContextProxy::resourceLoadDidCompleteWithError(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceResponse& response, const WebCore::ResourceError& error, const ResourceLoadInfo& resourceLoad)
{
    auto *details = webRequestDetailsForResourceLoad(resourceLoad, tabID);

    if (!error.isNull()) {
        [details addEntriesFromDictionary:@{
            @"tabId": @(toWebAPI(tabID)),
            @"error": @"net::ERR_ABORTED"
        }];

        enumerateNamespaceObjects([&](auto& namespaceObject) {
            auto& webRequestObject = namespaceObject.webRequest();
            webRequestObject.onErrorOccurred().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
        });
    } else {
        [details addEntriesFromDictionary:headersReceivedDetails(resourceLoad, tabID, response)];

        enumerateNamespaceObjects([&](auto& namespaceObject) {
            auto& webRequestObject = namespaceObject.webRequest();
            webRequestObject.onCompleted().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
        });
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
