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
#import "WebExtensionControllerProxy.h"
#import "WebExtensionFrameIdentifier.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionMessageTargetParameters.h"
#import "WebExtensionUtilities.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/SecurityOrigin.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>

static NSString * const idKey = @"id";
static NSString * const frameIdKey = @"frameId";
static NSString * const tabKey = @"tab";
static NSString * const urlKey = @"url";
static NSString * const originKey = @"origin";
static NSString * const nameKey = @"name";
static NSString * const reasonKey = @"reason";
static NSString * const previousVersionKey = @"previousVersion";
static NSString * const documentIdKey = @"documentId";

namespace WebKit {

enum class IsDefaultReply : bool { No, Yes };
using ReplyCallbackAggregator = EagerCallbackAggregator<void(id, IsDefaultReply)>;

}

@interface _WKReplyCallbackAggregator : NSObject

- (instancetype)initWithAggregator:(WebKit::ReplyCallbackAggregator&)aggregator;

@property (nonatomic, readonly) WebKit::ReplyCallbackAggregator& aggregator;

@end

@implementation _WKReplyCallbackAggregator {
    RefPtr<WebKit::ReplyCallbackAggregator> _aggregator;
}

- (instancetype)initWithAggregator:(WebKit::ReplyCallbackAggregator&)aggregator
{
    if (!(self = [super init]))
        return nil;

    _aggregator = &aggregator;

    return self;
}

- (WebKit::ReplyCallbackAggregator&)aggregator
{
    return *_aggregator;
}

@end

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

bool WebExtensionAPIRuntime::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    if (UNLIKELY(extensionContext().isUnsupportedAPI(propertyPath(), name)))
        return false;

    if (name == "connectNative"_s || name == "sendNativeMessage"_s)
        return extensionContext().hasPermission("nativeMessaging"_s);

    ASSERT_NOT_REACHED();
    return false;
}

NSURL *WebExtensionAPIRuntime::getURL(NSString *resourcePath, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getURL

    return URL { extensionContext().baseURL(), resourcePath };
}

NSDictionary *WebExtensionAPIRuntime::getManifest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getManifest

    return extensionContext().manifest();
}

NSString *WebExtensionAPIRuntime::runtimeIdentifier()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/id

    return extensionContext().uniqueIdentifier();
}

void WebExtensionAPIRuntime::getPlatformInfo(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getPlatformInfo

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

void WebExtensionAPIRuntime::getBackgroundPage(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getBackgroundPage

    if (auto backgroundPage = extensionContext().backgroundPage()) {
        callback->call(toWindowObject(callback->globalContext(), *backgroundPage) ?: NSNull.null);
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeGetBackgroundPage(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebCore::PageIdentifier>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        if (!result.value()) {
            callback->call(NSNull.null);
            return;
        }

        RefPtr page = WebProcess::singleton().webPage(result.value().value());
        if (!page) {
            callback->call(NSNull.null);
            return;
        }

        callback->call(toWindowObject(callback->globalContext(), *page) ?: NSNull.null);
    }, extensionContext().identifier());
}

double WebExtensionAPIRuntime::getFrameId(JSValue *target)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getFrameId

    if (!target)
        return WebExtensionFrameConstants::None;

    RefPtr frame = WebFrame::contentFrameForWindowOrFrameElement(target.context.JSGlobalContextRef, target.JSValueRef);
    if (!frame)
        return WebExtensionFrameConstants::None;

    return toWebAPI(toWebExtensionFrameIdentifier(*frame));
}

void WebExtensionAPIRuntime::setUninstallURL(URL, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/setUninstallURL

    // FIXME: rdar://58000001 Consider implementing runtime.setUninstallURL(), matching the behavior of other browsers.

    callback->call();
}

void WebExtensionAPIRuntime::openOptionsPage(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/openOptionsPage

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeOpenOptionsPage(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIRuntime::reload()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/reload

    WebProcess::singleton().send(Messages::WebExtensionContext::RuntimeReload(), extensionContext().identifier());
}

JSValue *WebExtensionAPIRuntime::lastError()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/lastError

    m_lastErrorAccessed = true;

    return m_lastError.get();
}

void WebExtensionAPIRuntime::sendMessage(WebPageProxyIdentifier webPageProxyIdentifier, WebFrame& frame, NSString *extensionID, NSString *messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage

    if (messageJSON.length > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nil, @"message", @"it exceeded the maximum allowed length");
        return;
    }

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nil, nil, @"an unexpected error occured");
        return;
    }

    // No options are supported currently.

    WebExtensionMessageSenderParameters senderParameters {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        webPageProxyIdentifier,
        contentWorldType(),
        frame.url(),
        documentIdentifier.value(),
    };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeSendMessage(extensionID, messageJSON, senderParameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(parseJSON(result.value(), JSONOptions::FragmentsAllowed));
    }, extensionContext().identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIRuntime::connect(WebPageProxyIdentifier webPageProxyIdentifier, WebFrame& frame, JSContextRef context, NSString *extensionID, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connect

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nil, nil, @"an unexpected error occured");
        return nullptr;
    }

    std::optional<String> name;
    if (!parseConnectOptions(options, name, @"options", outExceptionString))
        return nullptr;

    String resolvedName = name.value_or(nullString());

    WebExtensionMessageSenderParameters senderParameters {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        webPageProxyIdentifier,
        contentWorldType(),
        frame.url(),
        documentIdentifier.value(),
    };

    Ref port = WebExtensionAPIPort::create(*this, webPageProxyIdentifier, WebExtensionContentWorldType::Main, resolvedName);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeConnect(extensionID, port->channelIdentifier(), resolvedName, senderParameters), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](Expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(runtime().reportError(result.error(), globalContext.get()));
        port->disconnect();
    }, extensionContext().identifier());

    return port;
}

void WebExtensionAPIRuntime::sendNativeMessage(WebFrame& frame, NSString *applicationID, NSString *messageJSON, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendNativeMessage

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeSendNativeMessage(applicationID, messageJSON), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(parseJSON(result.value(), JSONOptions::FragmentsAllowed));
    }, extensionContext().identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIRuntime::connectNative(WebPageProxyIdentifier webPageProxyIdentifier, JSContextRef context, NSString *applicationID)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connectNative

    Ref port = WebExtensionAPIPort::create(*this, webPageProxyIdentifier, WebExtensionContentWorldType::Native, applicationID);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeConnectNative(applicationID, port->channelIdentifier(), webPageProxyIdentifier), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](Expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(runtime().reportError(result.error(), globalContext.get()));
        port->disconnect();
    }, extensionContext().identifier());

    return port;
}

void WebExtensionAPIWebPageRuntime::sendMessage(WebPage& page, WebFrame& frame, NSString *extensionID, NSString *messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage

    if (messageJSON.length > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nil, @"message", @"it exceeded the maximum allowed length");
        return;
    }

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nil, nil, @"an unexpected error occured");
        return;
    }

    WebExtensionMessageSenderParameters senderParameters {
        std::nullopt, // unique identifer
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        page.webPageProxyIdentifier(),
        WebExtensionContentWorldType::WebPage,
        frame.url(),
        documentIdentifier.value(),
    };

    RefPtr destinationExtensionContext = page.webExtensionControllerProxy()->extensionContext(extensionID);
    if (!destinationExtensionContext) {
        // Respond after a random delay to prevent the page from easily detecting if extensions are not installed.
        callAfterRandomDelay([callback = WTFMove(callback)]() {
            callback->call();
        });

        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeWebPageSendMessage(extensionID, messageJSON, senderParameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->call();
            return;
        }

        callback->call(parseJSON(result.value(), JSONOptions::FragmentsAllowed));
    }, destinationExtensionContext->identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIWebPageRuntime::connect(WebPage& page, WebFrame& frame, JSContextRef context, NSString *extensionID, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connect

    std::optional<String> name;
    if (!WebExtensionAPIRuntime::parseConnectOptions(options, name, @"options", outExceptionString))
        return nullptr;

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nil, nil, @"an unexpected error occured");
        return nullptr;
    }

    String resolvedName = name.value_or(nullString());

    WebExtensionMessageSenderParameters senderParameters {
        std::nullopt, // unique identifier
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        page.webPageProxyIdentifier(),
        WebExtensionContentWorldType::WebPage,
        frame.url(),
        documentIdentifier.value(),
    };

    RefPtr destinationExtensionContext = page.webExtensionControllerProxy()->extensionContext(extensionID);
    if (!destinationExtensionContext) {
        // Return a port that cant send messages, and disconnect after a random delay to prevent the page from easily detecting if extensions are not installed.
        Ref port = WebExtensionAPIPort::create(*this, resolvedName);

        callAfterRandomDelay([=]() {
            port->disconnect();
        });

        return port;
    }

    Ref port = WebExtensionAPIPort::create(contentWorldType(), runtime(), *destinationExtensionContext, page.webPageProxyIdentifier(), WebExtensionContentWorldType::Main, resolvedName);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeWebPageConnect(extensionID, port->channelIdentifier(), resolvedName, senderParameters), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](Expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(runtime().reportError(result.error(), globalContext.get()));
        port->disconnect();
    }, destinationExtensionContext->identifier());

    return port;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onMessage()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onMessage

    if (!m_onMessage)
        m_onMessage = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::RuntimeOnMessage);

    return *m_onMessage;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onConnect()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onConnect

    if (!m_onConnect)
        m_onConnect = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::RuntimeOnConnect);

    return *m_onConnect;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onInstalled()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onInstalled

    if (!m_onInstalled)
        m_onInstalled = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::RuntimeOnInstalled);

    return *m_onInstalled;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onStartup()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onStartup

    if (!m_onStartup)
        m_onStartup = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::RuntimeOnStartup);

    return *m_onStartup;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onConnectExternal()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onConnectExternal

    if (!m_onConnectExternal)
        m_onConnectExternal = WebExtensionAPIEvent::create(*this,  WebExtensionEventListenerType::RuntimeOnConnectExternal);

    return *m_onConnectExternal;
}

WebExtensionAPIEvent& WebExtensionAPIRuntime::onMessageExternal()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onMessageExternal

    if (!m_onMessageExternal)
        m_onMessageExternal = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::RuntimeOnMessageExternal);

    return *m_onMessageExternal;
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

    if (parameters.documentIdentifier.isValid())
        result[documentIdKey] = (NSString *)parameters.documentIdentifier.toString();

    return [result copy];
}

static bool matches(WebFrame& frame, const std::optional<WebExtensionMessageTargetParameters>& targetParameters)
{
    if (!targetParameters)
        return true;

    // Skip all frames / documents that don't match the target parameters.
    auto& frameIdentifier = targetParameters.value().frameIdentifier;
    if (frameIdentifier && !matchesFrame(frameIdentifier.value(), frame))
        return false;

    if (auto& documentIdentifier = targetParameters.value().documentIdentifier) {
        auto frameDocumentIdentifier = toDocumentIdentifier(frame);
        if (!frameDocumentIdentifier)
            return false;

        if (documentIdentifier != frameDocumentIdentifier)
            return false;
    }

    return true;
}

void WebExtensionContextProxy::internalDispatchRuntimeMessageEvent(WebExtensionContentWorldType contentWorldType, const String& messageJSON, const std::optional<WebExtensionMessageTargetParameters>& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(String&& replyJSON)>&& completionHandler)
{
    if (!hasDOMWrapperWorld(contentWorldType)) {
        // A null reply to the completionHandler means no listeners replied.
        completionHandler({ });
        return;
    }

    id message = parseJSON(messageJSON, JSONOptions::FragmentsAllowed);
    auto *senderInfo = toWebAPI(senderParameters);
    auto sourceContentWorldType = senderParameters.contentWorldType;

    auto callbackAggregator = ReplyCallbackAggregator::create([completionHandler = WTFMove(completionHandler)](JSValue *replyMessage, IsDefaultReply defaultReply) mutable {
        if (defaultReply == IsDefaultReply::Yes) {
            // A null reply to the completionHandler means no listeners replied.
            completionHandler({ });
            return;
        }

        auto *replyMessageJSON = encodeJSONString(replyMessage, JSONOptions::FragmentsAllowed);
        if (replyMessageJSON.length > webExtensionMaxMessageLength)
            replyMessageJSON = @"";

        // Ensure a real reply is never null, so the completionHandler can make the distinction.
        if (!replyMessageJSON)
            replyMessageJSON = @"";

        completionHandler(replyMessageJSON);
    }, nil, IsDefaultReply::Yes);

    // This ObjC wrapper is need for the inner reply block, which is required to be a compiled block.
    auto *callbackAggregatorWrapper = [[_WKReplyCallbackAggregator alloc] initWithAggregator:callbackAggregator];

    bool anyListenerHandledMessage = false;
    enumerateFramesAndNamespaceObjects([&, callbackAggregatorWrapper = RetainPtr { callbackAggregatorWrapper }](WebFrame& frame, WebExtensionAPINamespace& namespaceObject) {
        // Don't send the message to any listeners in the sender's page.
        if (senderParameters.pageProxyIdentifier == frame.protectedPage()->webPageProxyIdentifier())
            return;

        // Skip all frames that don't match the target parameters.
        if (!matches(frame, targetParameters))
            return;

        WebExtensionAPIEvent::ListenerVector listeners;
        if (sourceContentWorldType == WebExtensionContentWorldType::WebPage)
            listeners = namespaceObject.runtime().onMessageExternal().listeners();
        else
            listeners = namespaceObject.runtime().onMessage().listeners();

        if (listeners.isEmpty())
            return;

        for (auto& listener : listeners) {
            // Using BlockPtr for this call does not work, since JSValue needs a compiled block
            // with a signature to translate the JS function arguments. Having the block capture
            // callbackAggregatorWrapper ensures that callbackAggregator remains in scope.
            id returnValue = listener->call(message, senderInfo, ^(JSValue *replyMessage) {
                callbackAggregatorWrapper.get().aggregator(replyMessage, IsDefaultReply::No);
            });

            if (dynamic_objc_cast<NSNumber>(returnValue).boolValue) {
                anyListenerHandledMessage = true;
                continue;
            }

            JSValue *value = dynamic_objc_cast<JSValue>(returnValue);
            if (!value._isThenable)
                continue;

            anyListenerHandledMessage = true;

            [value _awaitThenableResolutionWithCompletionHandler:^(JSValue *replyMessage, id error) {
                if (error)
                    return;

                callbackAggregatorWrapper.get().aggregator(replyMessage, IsDefaultReply::No);
            }];
        }
    }, toDOMWrapperWorld(contentWorldType));

    if (!anyListenerHandledMessage)
        callbackAggregator.get()(nil, IsDefaultReply::Yes);
}

void WebExtensionContextProxy::dispatchRuntimeMessageEvent(WebExtensionContentWorldType contentWorldType, const String& messageJSON, const std::optional<WebExtensionMessageTargetParameters>& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(String&& replyJSON)>&& completionHandler)
{
    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        internalDispatchRuntimeMessageEvent(contentWorldType, messageJSON, targetParameters, senderParameters, WTFMove(completionHandler));
        return;

    case WebExtensionContentWorldType::ContentScript:
        internalDispatchRuntimeMessageEvent(contentWorldType, messageJSON, targetParameters, senderParameters, WTFMove(completionHandler));
        return;

    case WebExtensionContentWorldType::Native:
    case WebExtensionContentWorldType::WebPage:
        ASSERT_NOT_REACHED();
        return;
    }
}

void WebExtensionContextProxy::internalDispatchRuntimeConnectEvent(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const std::optional<WebExtensionMessageTargetParameters>& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(HashCountedSet<WebPageProxyIdentifier>&&)>&& completionHandler)
{
    if (!hasDOMWrapperWorld(contentWorldType)) {
        completionHandler({ });
        return;
    }

    HashCountedSet<WebPageProxyIdentifier> firedEventCounts;
    auto sourceContentWorldType = senderParameters.contentWorldType;

    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        // Don't send the event to any listeners in the sender's page.
        auto webPageProxyIdentifier = frame.page()->webPageProxyIdentifier();
        if (senderParameters.pageProxyIdentifier == webPageProxyIdentifier)
            return;

        // Skip all frames that don't match the target parameters.
        if (!matches(frame, targetParameters))
            return;

        WebExtensionAPIEvent::ListenerVector listeners;
        if (sourceContentWorldType == WebExtensionContentWorldType::WebPage)
            listeners = namespaceObject.runtime().onConnectExternal().listeners();
        else
            listeners = namespaceObject.runtime().onConnect().listeners();

        if (listeners.isEmpty())
            return;

        firedEventCounts.add(webPageProxyIdentifier, listeners.size());

        auto globalContext = frame.jsContextForWorld(toDOMWrapperWorld(contentWorldType));
        for (auto& listener : listeners) {
            Ref port = WebExtensionAPIPort::create(namespaceObject, frame.protectedPage()->webPageProxyIdentifier(), sourceContentWorldType, channelIdentifier, name, senderParameters);
            listener->call(toJSValue(globalContext, toJS(globalContext, port.ptr())));
        }
    }, toDOMWrapperWorld(contentWorldType));

    completionHandler(WTFMove(firedEventCounts));
}

void WebExtensionContextProxy::dispatchRuntimeConnectEvent(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const std::optional<WebExtensionMessageTargetParameters>& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(HashCountedSet<WebPageProxyIdentifier>&&)>&& completionHandler)
{
    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        internalDispatchRuntimeConnectEvent(contentWorldType, channelIdentifier, name, targetParameters, senderParameters, WTFMove(completionHandler));
        return;

    case WebExtensionContentWorldType::ContentScript:
        internalDispatchRuntimeConnectEvent(contentWorldType, channelIdentifier, name, targetParameters, senderParameters, WTFMove(completionHandler));
        return;

    case WebExtensionContentWorldType::Native:
    case WebExtensionContentWorldType::WebPage:
        ASSERT_NOT_REACHED();
        return;
    }
}

inline NSString *toWebAPI(WebExtensionContext::InstallReason installReason)
{
    switch (installReason) {
    case WebExtensionContext::InstallReason::None:
        ASSERT_NOT_REACHED();
        return nil;

    case WebExtensionContext::InstallReason::ExtensionInstall:
        return @"install";

    case WebExtensionContext::InstallReason::ExtensionUpdate:
        return @"update";

    case WebExtensionContext::InstallReason::BrowserUpdate:
        return @"browser_update";
    }
}

void WebExtensionContextProxy::dispatchRuntimeInstalledEvent(WebExtensionContext::InstallReason installReason, String previousVersion)
{
    NSDictionary *details;

    if (installReason == WebExtensionContext::InstallReason::ExtensionUpdate)
        details = @{ reasonKey: toWebAPI(installReason), previousVersionKey: (NSString *)previousVersion };
    else
        details = @{ reasonKey: toWebAPI(installReason) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.runtime().onInstalled().invokeListenersWithArgument(details);
    });
}

void WebExtensionContextProxy::dispatchRuntimeStartupEvent()
{
    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.runtime().onStartup().invokeListeners();
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
