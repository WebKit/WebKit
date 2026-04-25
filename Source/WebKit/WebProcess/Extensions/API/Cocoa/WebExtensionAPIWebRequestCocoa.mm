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
#import "FormDataReference.h"
#import "ResourceLoadInfo.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIWebRequest.h"
#import <WebCore/HTTPStatusCodes.h>
#import <WebCore/ResourceResponse.h>
#import <wtf/URLParser.h>

#if ENABLE(WK_WEB_EXTENSIONS)

static NSString * const formDataKey = @"formData";
static NSString * const rawKey = @"raw";
static NSString * const bytesKey = @"bytes";
static NSString * const errorKey = @"error";

static NSString * const parentFrameIdKey = @"parentFrameId";
static NSString * const requestIdKey = @"requestId";
static NSString * const timeStampKey = @"timeStamp";
static NSString * const typeKey = @"type";
static NSString * const methodKey = @"method";
static NSString * const redirectURLKey = @"redirectUrl";

static NSString * const valueKey = @"value";

static NSString * const statusLineKey = @"statusLine";
static NSString * const statusCodeKey = @"statusCode";
static NSString * const fromCacheKey = @"fromCache";

static NSString * const schemeKey = @"scheme";
static NSString * const challengerKey = @"challenger";
static NSString * const hostKey = @"host";
static NSString * const portKey = @"port";
static NSString * const isProxyKey = @"isProxy";
static NSString * const digestSchemeKey = @"digest";
static NSString * const basicSchemeKey = @"basic";
static NSString * const realmKey = @"realm";

static NSString * const requestBodyKey = @"requestBody";
static NSString * const requestHeadersKey = @"requestHeaders";
static NSString * const responseHeadersKey = @"responseHeaders";

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

static NSString *toWebAPI(const WebCore::FormData& formData, const String& contentType, JSGlobalContextRef context)
{
    if (formData.isEmpty() || !context)
        return nil;

    auto *result = [NSMutableDictionary dictionary];
    auto *formDataDictionary = [NSMutableDictionary dictionary];
    auto *rawArray = [NSMutableArray array];

    bool isURLEncoded = equalLettersIgnoringASCIICase(contentType, "application/x-www-form-urlencoded"_s);

    for (const WebCore::FormDataElement& element : formData.elements()) {
        if (auto* data = std::get_if<Vector<uint8_t>>(&element.data)) {
            if (isURLEncoded) {
                auto dataString = String::fromUTF8(data->span());

                for (auto& pair : URLParser::parseURLEncodedForm(dataString)) {
                    RetainPtr key = pair.key.createNSString();
                    RetainPtr value = pair.value.createNSString();

                    NSMutableArray *array = formDataDictionary[key.get()];
                    if (!array) {
                        array = [NSMutableArray array];
                        formDataDictionary[key.get()] = array;
                    }

                    [array addObject:value.get() ?: @""];
                }
            } else {
                auto typedArray = JSObjectMakeTypedArray(context, kJSTypedArrayTypeUint8Array, data->size(), nullptr);
                if (!typedArray) {
                    RELEASE_LOG_ERROR(Extensions, "Error creating Typed Array for raw data.");
                    continue;
                }

                uint8_t *typedArrayData = reinterpret_cast<uint8_t *>(JSObjectGetTypedArrayBytesPtr(context, typedArray, nullptr));
                if (!typedArrayData) {
                    RELEASE_LOG_ERROR(Extensions, "Error accessing Typed Array backing store.");
                    continue;
                }

                auto typedArraySpan = unsafeMakeSpan(typedArrayData, data->size());
                memcpySpan(typedArraySpan, data->span());

                [rawArray addObject:@{ bytesKey: [JSValue valueWithJSValueRef:typedArray inContext:[JSContext contextWithJSGlobalContextRef:context]] }];
            }
        }
    }

    if (formDataDictionary.count)
        result[formDataKey] = [formDataDictionary copy];

    if (rawArray.count)
        result[rawKey] = [rawArray copy];

    if (!formDataDictionary.count && !rawArray.count)
        result[errorKey] = @"Request body data is malformed or unsupported.";

    return [result copy];
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
        parentFrameIdKey: resourceLoad.parentFrameID ? @(toWebAPI(toWebExtensionFrameIdentifier(resourceLoad.parentFrameID))) : @(toWebAPI(WebExtensionFrameConstants::NoneIdentifier)),
        requestIdKey: adoptNS([[NSString alloc] initWithFormat:@"%llu", resourceLoad.resourceLoadID.toUInt64()]).get(),
        timeStampKey: @(floor(resourceLoad.eventTimestamp.approximateWallTime().secondsSinceEpoch().milliseconds())),
        @"url": resourceLoad.originalURL.string().createNSString().get(),
        @"tabId": @(toWebAPI(tabIdentifier)),
        typeKey: toWebAPI(resourceLoad.type),
        methodKey: resourceLoad.originalHTTPMethod.createNSString().get(),
    } mutableCopy];

    if (resourceLoad.documentID)
        result[@"documentId"] = resourceLoad.documentID.value().toString().createNSString().get();

    return result;
}

static NSArray *convertHeaderFieldsToWebExtensionFormat(const WebCore::HTTPHeaderMap& headerMap)
{
    auto *convertedHeaderFields = [NSMutableArray arrayWithCapacity:headerMap.size()];
    for (auto& header : headerMap) {
        [convertedHeaderFields addObject:@{
            @"name": header.key.createNSString().get(),
            valueKey: header.value.createNSString().get()
        }];
    }

    // FIXME: <rdar://problem/58967376> Add cookies.

    return [convertedHeaderFields copy];
}

static NSMutableDictionary *headersReceivedDetails(const ResourceLoadInfo& resourceLoad, WebExtensionTabIdentifier tabID, const WebCore::ResourceResponse& response)
{
    auto *details = webRequestDetailsForResourceLoad(resourceLoad, tabID);

    [details addEntriesFromDictionary:@{
        statusLineKey: response.httpStatusText().createNSString().get(),
        statusCodeKey: @(response.httpStatusCode()),
        fromCacheKey: @(resourceLoad.loadedFromCache),
        // FIXME: <rdar://problem/57132290> Add ip.
    }];

    return details;
}

void WebExtensionContextProxy::resourceLoadDidSendRequest(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceRequest& request, const ResourceLoadInfo& resourceLoad, const std::optional<IPC::FormDataReference>& formDataOptional)
{
    auto *details = webRequestDetailsForResourceLoad(resourceLoad, tabID);

    RefPtr formData = formDataOptional ? formDataOptional->data() : nullptr;
    auto contentType = request.httpContentType();

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.webRequest().onBeforeRequest().enumerateListeners(tabID, windowID, resourceLoad, [&](auto& listener, auto& extraInfo) {
            if (formData && extraInfo.contains(String(requestBodyKey))) {
                if (auto *requestBody = toWebAPI(*formData, contentType, listener.globalContext()))
                    [details setObject:requestBody forKey:requestBodyKey];
            }

            listener.call(toJSValueRef(listener.globalContext(), details));

            [details removeObjectForKey:requestBodyKey];
        });
    });

    RetainPtr<NSArray> requestHeaders;

    auto handleListeners = [&](auto& listeners) {
        listeners.enumerateListeners(tabID, windowID, resourceLoad, [&](auto& listener, auto& extraInfo) {
            if (extraInfo.contains(String(requestHeadersKey))) {
                if (!requestHeaders)
                    requestHeaders = convertHeaderFieldsToWebExtensionFormat(request.httpHeaderFields());
                [details setObject:requestHeaders.get() forKey:requestHeadersKey];
            }

            listener.call(toJSValueRef(listener.globalContext(), details));

            [details removeObjectForKey:requestHeadersKey];
        });
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        handleListeners(namespaceObject.webRequest().onBeforeSendHeaders());
        handleListeners(namespaceObject.webRequest().onSendHeaders());
    });
}

void WebExtensionContextProxy::resourceLoadDidPerformHTTPRedirection(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceResponse& response, const ResourceLoadInfo& resourceLoad, const WebCore::ResourceRequest& newRequest)
{
    auto *details = headersReceivedDetails(resourceLoad, tabID, response);
    NSArray *responseHeaders;

    auto handleListeners = [&](auto& listeners) {
        listeners.enumerateListeners(tabID, windowID, resourceLoad, [&](auto& listener, auto& extraInfo) {
            if (extraInfo.contains(String(responseHeadersKey))) {
                if (!responseHeaders)
                    responseHeaders = convertHeaderFieldsToWebExtensionFormat(response.httpHeaderFields());
                [details setObject:responseHeaders forKey:responseHeadersKey];
            }

            listener.call(toJSValueRef(listener.globalContext(), details));

            [details removeObjectForKey:responseHeadersKey];
        });
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        handleListeners(namespaceObject.webRequest().onHeadersReceived());
    });

    details[redirectURLKey] = newRequest.url().string().createNSString().get();

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        handleListeners(namespaceObject.webRequest().onBeforeRedirect());
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
        schemeKey: [challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodHTTPDigest] ? digestSchemeKey : basicSchemeKey,
        challengerKey: @{
            hostKey: challenge.protectionSpace.host,
            portKey: @(challenge.protectionSpace.port)
        },
        isProxyKey: @(challenge.protectionSpace.isProxy),
    }];

    if (auto *realm = challenge.protectionSpace.realm)
        details[realmKey] = realm;

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.webRequest().onAuthRequired().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
    });
}

void WebExtensionContextProxy::resourceLoadDidReceiveResponse(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceResponse& response, const ResourceLoadInfo& resourceLoad)
{
    auto *details = headersReceivedDetails(resourceLoad, tabID, response);
    NSArray *responseHeaders;

    auto handleListeners = [&](auto& listeners) {
        listeners.enumerateListeners(tabID, windowID, resourceLoad, [&](auto& listener, auto& extraInfo) {
            if (extraInfo.contains(String(responseHeadersKey))) {
                if (!responseHeaders)
                    responseHeaders = convertHeaderFieldsToWebExtensionFormat(response.httpHeaderFields());
                [details setObject:responseHeaders forKey:responseHeadersKey];
            }

            listener.call(toJSValueRef(listener.globalContext(), details));

            [details removeObjectForKey:responseHeadersKey];
        });
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        handleListeners(namespaceObject.webRequest().onHeadersReceived());
        handleListeners(namespaceObject.webRequest().onResponseStarted());
    });
}

void WebExtensionContextProxy::resourceLoadDidCompleteWithError(WebExtensionTabIdentifier tabID, WebExtensionWindowIdentifier windowID, const WebCore::ResourceResponse& response, const WebCore::ResourceError& error, const ResourceLoadInfo& resourceLoad)
{
    auto *details = webRequestDetailsForResourceLoad(resourceLoad, tabID);

    if (!error.isNull()) {
        [details addEntriesFromDictionary:@{
            @"tabId": @(toWebAPI(tabID)),
            errorKey: @"net::ERR_ABORTED"
        }];

        enumerateNamespaceObjects([&](auto& namespaceObject) {
            namespaceObject.webRequest().onErrorOccurred().invokeListenersWithArgument(details, tabID, windowID, resourceLoad);
        });

        return;
    }

    [details addEntriesFromDictionary:headersReceivedDetails(resourceLoad, tabID, response)];

    NSArray *responseHeaders;

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.webRequest().onCompleted().enumerateListeners(tabID, windowID, resourceLoad, [&](auto& listener, auto& extraInfo) {
            if (extraInfo.contains(String(responseHeadersKey))) {
                if (!responseHeaders)
                    responseHeaders = convertHeaderFieldsToWebExtensionFormat(response.httpHeaderFields());
                [details setObject:responseHeaders forKey:responseHeadersKey];
            }

            listener.call(toJSValueRef(listener.globalContext(), details));

            [details removeObjectForKey:responseHeadersKey];
        });
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
