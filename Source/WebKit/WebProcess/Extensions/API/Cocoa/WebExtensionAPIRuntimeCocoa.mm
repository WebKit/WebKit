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

namespace WebKit {

void WebExtensionAPIRuntimeBase::reportErrorForCallbackHandler(WebExtensionCallbackHandler& callback, NSString *errorMessage, JSGlobalContextRef contextRef)
{
    ASSERT(errorMessage.length);
    ASSERT(contextRef);

    RELEASE_LOG_ERROR(Extensions, "Callback error reported: %{public}@", errorMessage);

    JSContext *context = [JSContext contextWithJSGlobalContextRef:contextRef];

    m_lastErrorAccessed = false;
    m_lastError = [JSValue valueWithNewErrorFromMessage:errorMessage inContext:context];

    callback.call();

    if (!m_lastErrorAccessed) {
        // Log the error to the console if it wasn't checked in the callback.
        JSValue *consoleErrorFunction = context.globalObject[@"console"][@"error"];
        [consoleErrorFunction callWithArguments:@[ [JSValue valueWithNewErrorFromMessage:[NSString stringWithFormat:@"Unchecked runtime.lastError: %@", errorMessage] inContext:context] ]];

        RELEASE_LOG_DEBUG(Extensions, "Unchecked runtime.lastError");
    }

    m_lastErrorAccessed = false;
    m_lastError = nil;
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

JSValueRef WebExtensionAPIRuntime::lastError()
{
    m_lastErrorAccessed = true;

    return m_lastError.get().JSValueRef;
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

        callback->call(parseJSON(replyJSON.value()));
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onMessage()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onMessage

    if (!m_onMessage)
        m_onMessage = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::RuntimeOnMessage);

    return *m_onMessage;
}

static NSDictionary *toWebAPI(const WebExtensionMessageSenderParameters& parameters)
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

void WebExtensionContextProxy::dispatchRuntimeMessageEvent(WebCore::DOMWrapperWorld& world, String messageJSON, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON)>&& completionHandler)
{
    auto *message = parseJSON(messageJSON);
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
            replyMessageJSON = encodeJSONString(replyMessage);

        if (replyMessageJSON.length > webExtensionMaxMessageLength)
            replyMessageJSON = nil;

        sentReply = true;
        completionHandler(replyMessageJSON ? std::optional(String(replyMessageJSON)) : std::nullopt);
    };

    enumerateFramesAndNamespaceObjects(makeBlockPtr(^(WebFrame& frame, WebExtensionAPINamespace& namespaceObject) {
        // Skip all frames that don't match if a target frame identifier is specified.
        if (frameIdentifier && !matchesFrame(frameIdentifier.value(), frame))
            return;

        // Don't send the message back to the sender.
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

void WebExtensionContextProxy::dispatchRuntimeMainWorldMessageEvent(String messageJSON, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON)>&& completionHandler)
{
    dispatchRuntimeMessageEvent(mainWorld(), messageJSON, std::nullopt, senderParameters, WTFMove(completionHandler));
}

void WebExtensionContextProxy::dispatchRuntimeContentScriptMessageEvent(String messageJSON, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON)>&& completionHandler)
{
    dispatchRuntimeMessageEvent(contentScriptWorld(), messageJSON, frameIdentifier, senderParameters, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
