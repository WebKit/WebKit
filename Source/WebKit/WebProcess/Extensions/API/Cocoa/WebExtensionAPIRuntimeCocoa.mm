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
#import "WebExtensionAPIRuntime.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIEvent.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIPort.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <WebCore/SecurityOrigin.h>
#import <wtf/BlockPtr.h>

static NSString * const idKey = @"id";
static NSString * const frameIdKey = @"frameId";
static NSString * const tabKey = @"tab";
static NSString * const urlKey = @"url";
static NSString * const originKey = @"origin";
static NSString * const nameKey = @"name";

namespace WebKit {

JSValue *WebExtensionAPIRuntimeBase::reportError(NSString *errorMessage, JSGlobalContextRef contextRef, Function<void()>&& handler)
{
    ASSERT(errorMessage.length);
    ASSERT(contextRef);

    RELEASE_LOG_ERROR(Extensions, "Runtime error reported: %{public}@", errorMessage);

    JSContext *context = [JSContext contextWithJSGlobalContextRef:contextRef];

    auto *result = [JSValue valueWithNewErrorFromMessage:errorMessage inContext:context];

    m_lastErrorAccessed = false;
    m_lastError = result;

    if (handler) {
        errorMessage = [@"Unchecked runtime.lastError: " stringByAppendingString:errorMessage];
        handler();
    }

    if (!m_lastErrorAccessed) {
        // Log the error to the console if it wasn't checked in the callback.
        JSValue *consoleErrorFunction = context.globalObject[@"console"][@"error"];
        [consoleErrorFunction callWithArguments:@[[JSValue valueWithNewErrorFromMessage:errorMessage inContext:context]]];

        if (handler)
            RELEASE_LOG_DEBUG(Extensions, "Unchecked runtime.lastError");
    }

    m_lastErrorAccessed = false;
    m_lastError = nil;

    return result;
}

JSValue *WebExtensionAPIRuntimeBase::reportError(NSString *errorMessage, WebExtensionCallbackHandler& callback)
{
    return reportError(errorMessage, callback.globalContext(), [&]() {
        callback.call();
    });
}

bool WebExtensionAPIRuntime::parseConnectOptions(NSDictionary *options, std::optional<String>& name, NSString *sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *nameString = options[nameKey])
        name = nameString;

    return true;
}

NSURL *WebExtensionAPIRuntime::getURL(NSString *resourcePath, NSString **errorString)
{
    URL baseURL = extensionContext().baseURL();
    return resourcePath.length ? URL { baseURL, resourcePath } : baseURL;
}

NSDictionary *WebExtensionAPIRuntime::getManifest()
{
    return extensionContext().manifest();
}

NSString *WebExtensionAPIRuntime::runtimeIdentifier()
{
    return extensionContext().uniqueIdentifier();
}

void WebExtensionAPIRuntime::getPlatformInfo(Ref<WebExtensionCallbackHandler>&& callback)
{
#if PLATFORM(MAC)
    static NSString * const osValue = @"mac";
#elif PLATFORM(IOS_FAMILY)
    static NSString * const osValue = @"ios";
#else
    static NSString * const osValue = @"unknown";
#endif

#if CPU(X86_64)
    static NSString * const archValue = @"x86-64";
#elif CPU(ARM) || CPU(ARM64)
    static NSString * const archValue = @"arm";
#else
    static NSString * const archValue = @"unknown";
#endif

    static NSDictionary *platformInfo = @{
        @"os": osValue,
        @"arch": archValue
    };

    callback->call(platformInfo);
}

JSValue *WebExtensionAPIRuntime::lastError()
{
    m_lastErrorAccessed = true;

    return m_lastError.get();
}

void WebExtensionAPIRuntime::sendMessage(WebFrame *frame, NSString *extensionID, NSString *message, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage

    if (message.length > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nil, @"message", @"it exceeded the maximum allowed length");
        return;
    }

    // No options are supported currently.

    WebExtensionMessageSenderParameters sender {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(*frame),
        frame->page()->webPageProxyIdentifier(),
        contentWorldType(),
        frame->url(),
    };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeSendMessage(extensionID, message, sender), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> replyJSON, std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        if (!replyJSON) {
            callback->call();
            return;
        }

        callback->call(parseJSON(replyJSON.value(), { JSONOptions::FragmentsAllowed }));
    }, extensionContext().identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIRuntime::connect(WebFrame* frame, JSContextRef context, NSString *extensionID, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connect

    std::optional<String> name;
    if (!parseConnectOptions(options, name, @"options", outExceptionString))
        return nullptr;

    String resolvedName = name.value_or(nullString());

    WebExtensionMessageSenderParameters sender {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(*frame),
        frame->page()->webPageProxyIdentifier(),
        contentWorldType(),
        frame->url(),
    };

    auto port = WebExtensionAPIPort::create(forMainWorld(), runtime(), extensionContext(), WebExtensionContentWorldType::Main, resolvedName, sender);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeConnect(extensionID, port->channelIdentifier(), resolvedName, sender), [=, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }, protectedThis = Ref { *this }](WebExtensionTab::Error error) {
        if (!error)
            return;

        port->setError(protectedThis->runtime().reportError(error.value(), globalContext.get()));
        port->disconnect();
    }, extensionContext().identifier());

    return port;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onMessage()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onMessage

    if (!m_onMessage)
        m_onMessage = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::RuntimeOnMessage);

    return *m_onMessage;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onConnect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onConnect

    if (!m_onConnect)
        m_onConnect = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::RuntimeOnConnect);

    return *m_onConnect;
}

NSDictionary *toWebAPI(const WebExtensionMessageSenderParameters& parameters)
{
    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    if (parameters.extensionUniqueIdentifier)
        result[idKey] = (NSString *)parameters.extensionUniqueIdentifier.value();

    if (parameters.tabParameters)
        result[tabKey] = toWebAPI(parameters.tabParameters.value());

    // The frame identifier is only included when tab is included.
    if (parameters.frameIdentifier && parameters.tabParameters)
        result[frameIdKey] = @(toWebAPI(parameters.frameIdentifier.value()));

    if (parameters.url.isValid()) {
        result[urlKey] = (NSString *)parameters.url.string();
        result[originKey] = (NSString *)WebCore::SecurityOrigin::create(parameters.url)->toString();
    }

    return [result copy];
}

void WebExtensionContextProxy::internalDispatchRuntimeMessageEvent(WebCore::DOMWrapperWorld& world, const String& messageJSON, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON)>&& completionHandler)
{
    id message = parseJSON(messageJSON, { JSONOptions::FragmentsAllowed });
    auto *senderInfo = toWebAPI(senderParameters);

    __block bool anyListenerHandledMessage = false;
    bool sentReply = false;

    __block auto handleReply = [&sentReply, completionHandler = WTFMove(completionHandler)](id replyMessage) mutable {
        if (sentReply)
            return;

        NSString *replyMessageJSON;
        if (JSValue *value = dynamic_objc_cast<JSValue>(replyMessage))
            replyMessageJSON = value._toJSONString;
        else
            replyMessageJSON = encodeJSONString(replyMessage, { JSONOptions::FragmentsAllowed });

        if (replyMessageJSON.length > webExtensionMaxMessageLength)
            replyMessageJSON = nil;

        sentReply = true;
        completionHandler(replyMessageJSON ? std::optional(String(replyMessageJSON)) : std::nullopt);
    };

    enumerateFramesAndNamespaceObjects(makeBlockPtr(^(WebFrame& frame, WebExtensionAPINamespace& namespaceObject) {
        // Skip all frames that don't match if a target frame identifier is specified.
        if (frameIdentifier && !matchesFrame(frameIdentifier.value(), frame))
            return;

        // Don't send the message to any listeners in the sender's page.
        if (senderParameters.pageProxyIdentifier == frame.page()->webPageProxyIdentifier())
            return;

        for (auto& listener : namespaceObject.runtime().onMessage().listeners()) {
            id returnValue = listener->call(message, senderInfo, ^(id replyMessage) {
                handleReply(replyMessage);
            });

            if (dynamic_objc_cast<NSNumber>(returnValue).boolValue) {
                anyListenerHandledMessage = true;
                continue;
            }

            JSValue *value = dynamic_objc_cast<JSValue>(returnValue);
            if (!value._isThenable)
                continue;

            anyListenerHandledMessage = true;

            [value _awaitThenableResolutionWithCompletionHandler:^(id replyMessage, id error) {
                if (error)
                    return;

                handleReply(replyMessage);
            }];
        }
    }).get(), world);

    if (!sentReply && !anyListenerHandledMessage)
        handleReply(nil);
}

void WebExtensionContextProxy::dispatchRuntimeMessageEvent(WebExtensionContentWorldType contentWorldType, const String& messageJSON, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON)>&& completionHandler)
{
    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
        ASSERT(!frameIdentifier);
        internalDispatchRuntimeMessageEvent(mainWorld(), messageJSON, std::nullopt, senderParameters, WTFMove(completionHandler));
        return;

    case WebExtensionContentWorldType::ContentScript:
        internalDispatchRuntimeMessageEvent(contentScriptWorld(), messageJSON, frameIdentifier, senderParameters, WTFMove(completionHandler));
        return;
    }
}

void WebExtensionContextProxy::internalDispatchRuntimeConnectEvent(WebCore::DOMWrapperWorld& world, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(size_t firedEventCount)>&& completionHandler)
{
    size_t firedEventCount = 0;
    auto targetContentWorldType = senderParameters.contentWorldType;

    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        // Skip all frames that don't match if a target frame identifier is specified.
        if (frameIdentifier && !matchesFrame(frameIdentifier.value(), frame))
            return;

        // Don't send the event to any listeners in the sender's page.
        if (senderParameters.pageProxyIdentifier == frame.page()->webPageProxyIdentifier())
            return;

        auto& onConnect = namespaceObject.runtime().onConnect();
        if (onConnect.listeners().isEmpty())
            return;

        firedEventCount += onConnect.listeners().size();

        auto globalContext = frame.jsContextForWorld(world);
        for (auto& listener : onConnect.listeners()) {
            auto port = WebExtensionAPIPort::create(namespaceObject.forMainWorld(), namespaceObject.runtime(), *this, targetContentWorldType, channelIdentifier, name, senderParameters);
            listener->call(toJSValue(globalContext, toJS(globalContext, port.ptr())));
        }
    }, world);

    completionHandler(firedEventCount);
}

void WebExtensionContextProxy::dispatchRuntimeConnectEvent(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(size_t firedEventCount)>&& completionHandler)
{
    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
        ASSERT(!frameIdentifier);
        internalDispatchRuntimeConnectEvent(mainWorld(), channelIdentifier, name, std::nullopt, senderParameters, WTFMove(completionHandler));
        return;

    case WebExtensionContentWorldType::ContentScript:
        internalDispatchRuntimeConnectEvent(contentScriptWorld(), channelIdentifier, name, frameIdentifier, senderParameters, WTFMove(completionHandler));
        return;
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
