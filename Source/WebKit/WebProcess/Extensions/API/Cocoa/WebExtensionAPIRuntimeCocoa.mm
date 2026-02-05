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
#import <wtf/text/MakeString.h>

static NSString * const idKey = @"id";
static NSString * const frameIdKey = @"frameId";
static NSString * const tabKey = @"tab";
static NSString * const urlKey = @"url";
static NSString * const originKey = @"origin";
static NSString * const nameKey = @"name";
static NSString * const reasonKey = @"reason";
static NSString * const previousVersionKey = @"previousVersion";
static NSString * const documentIdKey = @"documentId";
static NSString * const versionKey = @"version";

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

    _aggregator = aggregator;

    return self;
}

- (WebKit::ReplyCallbackAggregator&)aggregator
{
    return *_aggregator;
}

@end

namespace WebKit {

JSValue *WebExtensionAPIRuntimeBase::reportError(String errorMessage, JSGlobalContextRef contextRef, NOESCAPE const Function<void()>& handler)
{
    ASSERT(!errorMessage.isEmpty());
    ASSERT(contextRef);

    RELEASE_LOG_ERROR(Extensions, "Runtime error reported: %" PUBLIC_LOG_STRING, errorMessage.utf8().data());

    JSContext *context = [JSContext contextWithJSGlobalContextRef:contextRef];

    auto *result = [JSValue valueWithNewErrorFromMessage:errorMessage.createNSString().get() inContext:context];

    m_lastErrorAccessed = false;
    m_lastError = result;

    if (handler) {
        errorMessage = makeString("Unchecked runtime.lastError: "_s, errorMessage);
        handler();
    }

    if (!m_lastErrorAccessed) {
        // Log the error to the console if it wasn't checked in the callback.
        JSValue *consoleErrorFunction = context.globalObject[@"console"][@"error"];
        [consoleErrorFunction callWithArguments:@[[JSValue valueWithNewErrorFromMessage:errorMessage.createNSString().get() inContext:context]]];

        if (handler)
            RELEASE_LOG_DEBUG(Extensions, "Unchecked runtime.lastError");
    }

    m_lastErrorAccessed = false;
    m_lastError = nil;

    return result;
}

JSValue *WebExtensionAPIRuntimeBase::reportError(const String& errorMessage, WebExtensionCallbackHandler& callback)
{
    return reportError(errorMessage, callback.globalContext(), [&]() {
        callback.call();
    });
}

bool WebExtensionAPIRuntime::parseConnectOptions(NSDictionary *options, std::optional<String>& name, const String& sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey.createNSString().get(), nil, types, outExceptionString))
        return false;

    if (NSString *nameString = options[nameKey])
        name = nameString;

    return true;
}

bool WebExtensionAPIRuntime::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    Ref extensionContext = this->extensionContext();
    if (extensionContext->isUnsupportedAPI(propertyPath(), name)) [[unlikely]]
        return false;

    if (name == "connectNative"_s || name == "sendNativeMessage"_s)
        return extensionContext->hasPermission("nativeMessaging"_s);

    ASSERT_NOT_REACHED();
    return false;
}

NSURL *WebExtensionAPIRuntime::getURL(const String& resourcePath, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getURL

    return URL { extensionContext().baseURL(), resourcePath }.createNSURL().autorelease();
}

NSDictionary *WebExtensionAPIRuntime::getManifest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getManifest

    return extensionContext().manifest();
}

String WebExtensionAPIRuntime::getVersion()
{
    return objectForKey<NSString>(extensionContext().manifest(), versionKey);
}

String WebExtensionAPIRuntime::runtimeIdentifier()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/id

    return extensionContext().uniqueIdentifier();
}

void WebExtensionAPIRuntime::getPlatformInfo(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getPlatformInfo

#if PLATFORM(MAC)
    static constexpr auto osValue = "mac"_s;
#elif PLATFORM(IOS_FAMILY)
    static constexpr auto osValue = "ios"_s;
#else
    static constexpr auto osValue = "unknown"_s;
#endif

#if CPU(X86_64)
    static constexpr auto archValue = "x86-64"_s;
#elif CPU(ARM) || CPU(ARM64)
    static constexpr auto archValue = "arm"_s;
#else
    static constexpr auto archValue = "unknown"_s;
#endif

    auto globalContext = callback->globalContext();
    callback->call(fromObject(callback->globalContext(), {
        { "os"_s, JSValueMakeString(globalContext, toJSString(osValue).get()) },
        { "arch"_s, JSValueMakeString(globalContext, toJSString(archValue).get()) }
    }));
}

void WebExtensionAPIRuntime::getBackgroundPage(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getBackgroundPage

    if (auto backgroundPage = protectedExtensionContext()->backgroundPage()) {
        callback->call(toWindowObject(callback->globalContext(), *backgroundPage));
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeGetBackgroundPage(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<std::optional<WebCore::PageIdentifier>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        if (!result.value()) {
            callback->call(JSValueMakeNull(callback->globalContext()));
            return;
        }

        RefPtr page = WebProcess::singleton().webPage(result.value().value());
        if (!page) {
            callback->call(JSValueMakeNull(callback->globalContext()));
            return;
        }

        callback->call(toWindowObject(callback->globalContext(), *page));
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

String WebExtensionAPIRuntime::getDocumentId(JSValue *target, NSString **outExceptionString)
{
    // Documentation: https://github.com/w3c/webextensions/blob/main/proposals/runtime_get_document_id.md

    RefPtr frame = target ? WebFrame::contentFrameForWindowOrFrameElement(target.context.JSGlobalContextRef, target.JSValueRef) : nullptr;
    if (!frame) {
        *outExceptionString = toErrorString(nullString(), @"target", @"is not a valid window or frame element").createNSString().autorelease();
        return String();
    }

    auto documentIdentifier = toDocumentIdentifier(*frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"an unexpected error occurred").createNSString().autorelease();
        return String();
    }

    return documentIdentifier.value().toString();
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeOpenOptionsPage(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
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

void WebExtensionAPIRuntime::sendMessage(WebPageProxyIdentifier webPageProxyIdentifier, WebFrame& frame, const String& extensionID, const String& messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage

    if (messageJSON.length() > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nullString(), @"message", @"it exceeded the maximum allowed length").createNSString().autorelease();
        return;
    }

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"an unexpected error occured").createNSString().autorelease();
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeSendMessage(extensionID, messageJSON, senderParameters), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(fromJSON(callback->globalContext(), JSON::Value::parseJSON(result.value())));
    }, extensionContext().identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIRuntime::connect(WebPageProxyIdentifier webPageProxyIdentifier, WebFrame& frame, JSContextRef context, const String& extensionID, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connect

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"an unexpected error occured").createNSString().autorelease();
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

        port->setError(protectedRuntime()->reportError(result.error().createNSString().get(), globalContext.get()));
        port->disconnect();
    }, extensionContext().identifier());

    return port;
}

void WebExtensionAPIRuntime::sendNativeMessage(WebFrame& frame, const String& applicationID, const String& messageJSON, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendNativeMessage

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeSendNativeMessage(applicationID, messageJSON), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(fromJSON(callback->globalContext(), JSON::Value::parseJSON(result.value())));
    }, extensionContext().identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIRuntime::connectNative(WebPageProxyIdentifier webPageProxyIdentifier, JSContextRef context, const String& applicationID)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connectNative

    Ref port = WebExtensionAPIPort::create(*this, webPageProxyIdentifier, WebExtensionContentWorldType::Native, applicationID);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeConnectNative(applicationID, port->channelIdentifier(), webPageProxyIdentifier), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](Expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(protectedRuntime()->reportError(result.error().createNSString().get(), globalContext.get()));
        port->disconnect();
    }, extensionContext().identifier());

    return port;
}

void WebExtensionAPIWebPageRuntime::sendMessage(WebPage& page, WebFrame& frame, const String& extensionID, const String& messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendMessage

    if (messageJSON.length() > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nullString(), @"message", @"it exceeded the maximum allowed length").createNSString().autorelease();
        return;
    }

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"an unexpected error occured").createNSString().autorelease();
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

    RefPtr destinationExtensionContext = protect(page.webExtensionControllerProxy())->extensionContext(extensionID);
    if (!destinationExtensionContext) {
        // Respond after a random delay to prevent the page from easily detecting if extensions are not installed.
        callAfterRandomDelay([callback = WTF::move(callback)]() {
            callback->call();
        });

        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeWebPageSendMessage(extensionID, messageJSON, senderParameters), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->call();
            return;
        }

        callback->call(fromJSON(callback->globalContext(), JSON::Value::parseJSON(result.value())));
    }, destinationExtensionContext->identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPIWebPageRuntime::connect(WebPage& page, WebFrame& frame, JSContextRef context, const String& extensionID, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connect

    std::optional<String> name;
    if (!WebExtensionAPIRuntime::parseConnectOptions(options, name, @"options", outExceptionString))
        return nullptr;

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(nullString(), nullString(), @"an unexpected error occured").createNSString().autorelease();
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

    RefPtr destinationExtensionContext = protect(page.webExtensionControllerProxy())->extensionContext(extensionID);
    if (!destinationExtensionContext) {
        // Return a port that cant send messages, and disconnect after a random delay to prevent the page from easily detecting if extensions are not installed.
        Ref port = WebExtensionAPIPort::create(*this, resolvedName);

        callAfterRandomDelay([=]() {
            port->disconnect();
        });

        return port;
    }

    Ref port = WebExtensionAPIPort::create(contentWorldType(), protectedRuntime(), *destinationExtensionContext, page.webPageProxyIdentifier(), WebExtensionContentWorldType::Main, resolvedName);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::RuntimeWebPageConnect(extensionID, port->channelIdentifier(), resolvedName, senderParameters), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](Expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(protectedRuntime()->reportError(result.error().createNSString().get(), globalContext.get()));
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
        result[idKey] = parameters.extensionUniqueIdentifier.value().createNSString().get();

    if (parameters.tabParameters)
        result[tabKey] = toWebAPI(parameters.tabParameters.value());

    // The frame identifier is only included when tab is included.
    if (parameters.frameIdentifier && parameters.tabParameters)
        result[frameIdKey] = @(toWebAPI(parameters.frameIdentifier.value()));

    if (parameters.url.isValid()) {
        result[urlKey] = parameters.url.string().createNSString().get();
        result[originKey] = WebCore::SecurityOrigin::create(parameters.url)->toString().createNSString().get();
    }

    if (parameters.documentIdentifier.isValid())
        result[documentIdKey] = parameters.documentIdentifier.toString().createNSString().get();

    return [result copy];
}

static bool matches(WebFrame& frame, const std::optional<WebExtensionMessageTargetParameters>& targetParameters)
{
    if (!targetParameters)
        return true;

    // Skip all pages / frames / documents that don't match the target parameters.
    auto& pageProxyIdentifier = targetParameters.value().pageProxyIdentifier;
    if (pageProxyIdentifier && pageProxyIdentifier != protect(frame.page())->webPageProxyIdentifier())
        return false;

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

    id message = parseJSON(messageJSON.createNSString().get(), JSONOptions::FragmentsAllowed);
    auto *senderInfo = toWebAPI(senderParameters);
    auto sourceContentWorldType = senderParameters.contentWorldType;

    auto callbackAggregator = ReplyCallbackAggregator::create([completionHandler = WTF::move(completionHandler)](JSValue *replyMessage, IsDefaultReply defaultReply) mutable {
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
        if (senderParameters.pageProxyIdentifier == protect(frame.page())->webPageProxyIdentifier())
            return;

        // Skip all frames that don't match the target parameters.
        if (!matches(frame, targetParameters))
            return;

        WebExtensionAPIEvent::ListenerVector listeners;
        if (sourceContentWorldType == WebExtensionContentWorldType::WebPage)
            listeners = namespaceObject.protectedRuntime()->onMessageExternal().listeners();
        else
            listeners = namespaceObject.protectedRuntime()->onMessage().listeners();

        if (listeners.isEmpty())
            return;

        for (auto& listener : listeners) {
            // Using BlockPtr for this call does not work, since JSValue needs a compiled block
            // with a signature to translate the JS function arguments. Having the block capture
            // callbackAggregatorWrapper ensures that callbackAggregator remains in scope.
            auto returnValue = listener->call(toJSValueRef(listener->globalContext(), message), toJSValueRef(listener->globalContext(), senderInfo), toJSValueRef(listener->globalContext(), ^(JSValue *replyMessage) {
                callbackAggregatorWrapper.get().aggregator(replyMessage, IsDefaultReply::No);
            }));

            if (JSValueIsBoolean(listener->globalContext(), returnValue) && JSValueToBoolean(listener->globalContext(), returnValue)) {
                anyListenerHandledMessage = true;
                continue;
            }

            JSValue *value = toJSValue(listener->globalContext(), returnValue);
            if (!isThenable(value.context.JSGlobalContextRef, value.JSValueRef))
                continue;

            anyListenerHandledMessage = true;

            auto resolveBlock = ^(JSValue *replyMessage) {
                callbackAggregatorWrapper.get().aggregator(replyMessage, IsDefaultReply::No);
            };

            auto rejectBlock = ^(JSValue *error) {
                return;
            };

            [value invokeMethod:@"then" withArguments:@[ resolveBlock, rejectBlock ]];
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
        internalDispatchRuntimeMessageEvent(contentWorldType, messageJSON, targetParameters, senderParameters, WTF::move(completionHandler));
        return;

    case WebExtensionContentWorldType::ContentScript:
        internalDispatchRuntimeMessageEvent(contentWorldType, messageJSON, targetParameters, senderParameters, WTF::move(completionHandler));
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
            Ref port = WebExtensionAPIPort::create(namespaceObject, protect(frame.page())->webPageProxyIdentifier(), sourceContentWorldType, channelIdentifier, name, senderParameters);
            listener->call(toJS(globalContext, port.ptr()));
        }
    }, toDOMWrapperWorld(contentWorldType));

    completionHandler(WTF::move(firedEventCounts));
}

void WebExtensionContextProxy::dispatchRuntimeConnectEvent(WebExtensionContentWorldType contentWorldType, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const std::optional<WebExtensionMessageTargetParameters>& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(HashCountedSet<WebPageProxyIdentifier>&&)>&& completionHandler)
{
    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        internalDispatchRuntimeConnectEvent(contentWorldType, channelIdentifier, name, targetParameters, senderParameters, WTF::move(completionHandler));
        return;

    case WebExtensionContentWorldType::ContentScript:
        internalDispatchRuntimeConnectEvent(contentWorldType, channelIdentifier, name, targetParameters, senderParameters, WTF::move(completionHandler));
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
        details = @{ reasonKey: toWebAPI(installReason), previousVersionKey: previousVersion.createNSString().get() };
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
