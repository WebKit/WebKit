/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKUserContentControllerInternal.h"

#import "APIContentWorld.h"
#import "APISerializedScriptValue.h"
#import "InjectUserScriptImmediately.h"
#import "WKContentRuleListInternal.h"
#import "WKContentWorldInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKNSArray.h"
#import "WKScriptMessageHandler.h"
#import "WKScriptMessageHandlerWithReply.h"
#import "WKScriptMessageInternal.h"
#import "WKUserScriptInternal.h"
#import "WKWebViewInternal.h"
#import "WebScriptMessageHandler.h"
#import "WebUserContentControllerProxy.h"
#import "_WKUserContentFilterInternal.h"
#import "_WKUserContentWorldInternal.h"
#import "_WKUserStyleSheetInternal.h"
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/SerializedScriptValue.h>
#import <WebCore/WebCoreObjCExtras.h>

@implementation WKUserContentController

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebUserContentControllerProxy>(self);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKUserContentController.class, self))
        return;

    _userContentControllerProxy->~WebUserContentControllerProxy();

    [super dealloc];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    return self;
}

- (NSArray *)userScripts
{
    return wrapper(_userContentControllerProxy->userScripts());
}

- (void)addUserScript:(WKUserScript *)userScript
{
    _userContentControllerProxy->addUserScript(*userScript->_userScript, WebKit::InjectUserScriptImmediately::No);
}

- (void)removeAllUserScripts
{
    _userContentControllerProxy->removeAllUserScripts();
}

- (void)addContentRuleList:(WKContentRuleList *)contentRuleList
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->addContentRuleList(*contentRuleList->_contentRuleList);
#endif
}

- (void)removeContentRuleList:(WKContentRuleList *)contentRuleList
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->removeContentRuleList(contentRuleList->_contentRuleList->name());
#endif
}

- (void)removeAllContentRuleLists
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->removeAllContentRuleLists();
#endif
}

class ScriptMessageHandlerDelegate final : public WebKit::WebScriptMessageHandler::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScriptMessageHandlerDelegate(WKUserContentController *controller, id <WKScriptMessageHandler> handler, NSString *name)
        : m_controller(controller)
        , m_handler(handler)
        , m_name(adoptNS([name copy]))
        , m_supportsAsyncReply(false)
    {
    }

    ScriptMessageHandlerDelegate(WKUserContentController *controller, id <WKScriptMessageHandlerWithReply> handler, NSString *name)
        : m_controller(controller)
        , m_handler(handler)
        , m_name(adoptNS([name copy]))
        , m_supportsAsyncReply(true)
    {
    }

    void didPostMessage(WebKit::WebPageProxy& page, WebKit::FrameInfoData&& frameInfoData, API::ContentWorld& world, WebCore::SerializedScriptValue& serializedScriptValue) final
    {
        @autoreleasepool {
            auto webView = page.cocoaView();
            if (!webView)
                return;
            RetainPtr<WKFrameInfo> frameInfo = wrapper(API::FrameInfo::create(WTFMove(frameInfoData), &page));
            id body = API::SerializedScriptValue::deserialize(serializedScriptValue, 0);
            auto message = adoptNS([[WKScriptMessage alloc] _initWithBody:body webView:webView.get() frameInfo:frameInfo.get() name:m_name.get() world:wrapper(world)]);
        
            [(id<WKScriptMessageHandler>)m_handler.get() userContentController:m_controller.get() didReceiveScriptMessage:message.get()];
        }
    }

    bool supportsAsyncReply() final
    {
        return m_supportsAsyncReply;
    }

    void didPostMessageWithAsyncReply(WebKit::WebPageProxy& page, WebKit::FrameInfoData&& frameInfoData, API::ContentWorld& world, WebCore::SerializedScriptValue& serializedScriptValue, Function<void(API::SerializedScriptValue*, const String&)>&& replyHandler) final
    {
        ASSERT(m_supportsAsyncReply);

        auto webView = page.cocoaView();
        if (!webView) {
            replyHandler(nullptr, "The WKWebView was deallocated before the message was delivered"_s);
            return;
        }

        auto finalizer = [](Function<void(API::SerializedScriptValue*, const String&)>& function) {
            function(nullptr, "WKWebView API client did not respond to this postMessage"_s);
        };
        __block auto handler = CompletionHandlerWithFinalizer<void(API::SerializedScriptValue*, const String&)>(WTFMove(replyHandler), WTFMove(finalizer));

        @autoreleasepool {
            RetainPtr<WKFrameInfo> frameInfo = wrapper(API::FrameInfo::create(WTFMove(frameInfoData), &page));
            id body = API::SerializedScriptValue::deserialize(serializedScriptValue, 0);
            auto message = adoptNS([[WKScriptMessage alloc] _initWithBody:body webView:webView.get() frameInfo:frameInfo.get() name:m_name.get() world:wrapper(world)]);

            [(id<WKScriptMessageHandlerWithReply>)m_handler.get() userContentController:m_controller.get() didReceiveScriptMessage:message.get() replyHandler:^(id result, NSString *errorMessage) {
                if (!handler)
                    [NSException raise:NSInternalInconsistencyException format:@"replyHandler passed to userContentController:didReceiveScriptMessage:replyHandler: should not be called twice"];

                if (errorMessage) {
                    handler(nullptr, errorMessage);
                    return;
                }

                auto serializedValue = API::SerializedScriptValue::createFromNSObject(result);
                if (!serializedValue) {
                    handler(nullptr, "The result value passed back from the WKWebView API client was unable to be serialized"_s);
                    return;
                }

                handler(serializedValue.get(), { });
            }];
        }
    }

private:
    RetainPtr<WKUserContentController> m_controller;
    RetainPtr<id> m_handler;
    RetainPtr<NSString> m_name;
    bool m_supportsAsyncReply;
};

- (void)_addScriptMessageHandler:(WebKit::WebScriptMessageHandler&)scriptMessageHandler
{
    if (!_userContentControllerProxy->addUserScriptMessageHandler(scriptMessageHandler))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", (NSString *)scriptMessageHandler.name()];
}

- (void)addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, API::ContentWorld::pageContentWorld());
    [self _addScriptMessageHandler:handler.get()];
}

- (void)addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler contentWorld:(WKContentWorld *)world name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, *world->_contentWorld);
    [self _addScriptMessageHandler:handler.get()];
}

- (void)addScriptMessageHandlerWithReply:(id <WKScriptMessageHandlerWithReply>)scriptMessageHandler contentWorld:(WKContentWorld *)world name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, *world->_contentWorld);
    [self _addScriptMessageHandler:handler.get()];
}

- (void)removeScriptMessageHandlerForName:(NSString *)name
{
    _userContentControllerProxy->removeUserMessageHandlerForName(name, API::ContentWorld::pageContentWorld());
}

- (void)removeScriptMessageHandlerForName:(NSString *)name contentWorld:(WKContentWorld *)contentWorld
{
    _userContentControllerProxy->removeUserMessageHandlerForName(name, *contentWorld->_contentWorld);
}

- (void)removeAllScriptMessageHandlersFromContentWorld:(WKContentWorld *)contentWorld
{
    _userContentControllerProxy->removeAllUserMessageHandlers(*contentWorld->_contentWorld);
}

- (void)removeAllScriptMessageHandlers
{
    _userContentControllerProxy->removeAllUserMessageHandlers();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_userContentControllerProxy;
}

@end

@implementation WKUserContentController (WKPrivate)

- (void)_removeUserScript:(WKUserScript *)userScript
{
    _userContentControllerProxy->removeUserScript(*userScript->_userScript);
}

- (void)_removeAllUserScriptsAssociatedWithContentWorld:(WKContentWorld *)contentWorld
{
    _userContentControllerProxy->removeAllUserScripts(*contentWorld->_contentWorld);
}

- (void)_addUserScriptImmediately:(WKUserScript *)userScript
{
    _userContentControllerProxy->addUserScript(*userScript->_userScript, WebKit::InjectUserScriptImmediately::Yes);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (void)_addUserContentFilter:(_WKUserContentFilter *)userContentFilter
#pragma clang diagnostic pop
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->addContentRuleList(*userContentFilter->_contentRuleList->_contentRuleList);
#endif
}

- (void)_addContentRuleList:(WKContentRuleList *)contentRuleList extensionBaseURL:(NSURL *)extensionBaseURL
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->addContentRuleList(*contentRuleList->_contentRuleList, extensionBaseURL);
#endif
}

- (void)_removeUserContentFilter:(NSString *)userContentFilterName
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->removeContentRuleList(userContentFilterName);
#endif
}

- (void)_removeAllUserContentFilters
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->removeAllContentRuleLists();
#endif
}

- (NSArray *)_userStyleSheets
{
    return wrapper(_userContentControllerProxy->userStyleSheets());
}

- (void)_addUserStyleSheet:(_WKUserStyleSheet *)userStyleSheet
{
    _userContentControllerProxy->addUserStyleSheet(*userStyleSheet->_userStyleSheet);
}

- (void)_removeUserStyleSheet:(_WKUserStyleSheet *)userStyleSheet
{
    _userContentControllerProxy->removeUserStyleSheet(*userStyleSheet->_userStyleSheet);
}

- (void)_removeAllUserStyleSheets
{
    _userContentControllerProxy->removeAllUserStyleSheets();
}

- (void)_removeAllUserStyleSheetsAssociatedWithContentWorld:(WKContentWorld *)contentWorld
{
    _userContentControllerProxy->removeAllUserStyleSheets(*contentWorld->_contentWorld);
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (void)_addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, *userContentWorld->_contentWorld->_contentWorld);
    if (!_userContentControllerProxy->addUserScriptMessageHandler(handler.get()))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", name];
}

- (void)_addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name contentWorld:(WKContentWorld *)contentWorld
{
    [self _addScriptMessageHandler:scriptMessageHandler name:name userContentWorld:contentWorld._userContentWorld];
}

- (void)_removeScriptMessageHandlerForName:(NSString *)name userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    _userContentControllerProxy->removeUserMessageHandlerForName(name, *userContentWorld->_contentWorld->_contentWorld);
}

- (void)_removeAllScriptMessageHandlersAssociatedWithUserContentWorld:(_WKUserContentWorld *)userContentWorld
{
    _userContentControllerProxy->removeAllUserMessageHandlers(*userContentWorld->_contentWorld->_contentWorld);
}
ALLOW_DEPRECATED_DECLARATIONS_END

@end
