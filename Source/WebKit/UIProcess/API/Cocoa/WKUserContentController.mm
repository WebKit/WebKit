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
#import "InjectUserScriptImmediately.h"
#import "JavaScriptEvaluationResult.h"
#import "WKContentRuleListInternal.h"
#import "WKContentWorldInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKJSScriptingBufferInternal.h"
#import "WKNSArray.h"
#import "WKScriptMessageHandler.h"
#import "WKScriptMessageHandlerWithReply.h"
#import "WKScriptMessageInternal.h"
#import "WKUserScriptInternal.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebScriptMessageHandler.h"
#import "WebUserContentControllerProxy.h"
#import "_WKJSBuffer.h"
#import "_WKUserContentFilterInternal.h"
#import "_WKUserContentWorldInternal.h"
#import "_WKUserStyleSheetInternal.h"
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/SerializedScriptValue.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/TZoneMallocInlines.h>

@implementation WKUserContentController

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

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

    SUPPRESS_UNRETAINED_ARG _userContentControllerProxy->~WebUserContentControllerProxy();

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
    protect(*_userContentControllerProxy)->addUserScript(Ref { *userScript->_userScript }, WebKit::InjectUserScriptImmediately::No);
}

- (void)removeAllUserScripts
{
    protect(*_userContentControllerProxy)->removeAllUserScripts();
}

- (void)addContentRuleList:(WKContentRuleList *)contentRuleList
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->addContentRuleList(Ref { *contentRuleList->_contentRuleList });
#endif
}

- (void)removeContentRuleList:(WKContentRuleList *)contentRuleList
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->removeContentRuleList(Ref { *contentRuleList->_contentRuleList }->name());
#endif
}

- (void)removeAllContentRuleLists
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->removeAllContentRuleLists();
#endif
}

class ScriptMessageHandlerDelegate final : public WebKit::WebScriptMessageHandler::Client {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ScriptMessageHandlerDelegate);
public:
    ScriptMessageHandlerDelegate(WKUserContentController *controller, id <WKScriptMessageHandler> handler, NSString *name)
        : m_controller(controller)
        , m_handler(handler)
        , m_name(name)
        , m_supportsAsyncReply(false)
    {
    }

    ScriptMessageHandlerDelegate(WKUserContentController *controller, id <WKScriptMessageHandlerWithReply> handler, NSString *name)
        : m_controller(controller)
        , m_handler(handler)
        , m_name(name)
        , m_supportsAsyncReply(true)
    {
    }

    void didPostMessage(WebKit::WebPageProxy& page, WebKit::FrameInfoData&& frameInfoData, API::ContentWorld& world, WebKit::JavaScriptEvaluationResult&& jsMessage, CompletionHandler<void(Expected<WebKit::JavaScriptEvaluationResult, String>&&)>&& replyHandler) final
    {
        @autoreleasepool {
            if (!page.cocoaView())
                return replyHandler(makeUnexpected("The WKWebView was deallocated before the message was delivered"_s));

            RetainPtr message = wrapper(API::ScriptMessage::create(jsMessage.toID(), page, API::FrameInfo::create(WTF::move(frameInfoData)), RetainPtr { m_name }, world));

            if (m_supportsAsyncReply) {
                __block auto handler = CompletionHandlerWithFinalizer<void(Expected<WebKit::JavaScriptEvaluationResult, String>&&)>(WTF::move(replyHandler), [](auto& function) {
                    function(makeUnexpected("WKWebView API client did not respond to this postMessage"_s));
                });
                [(id<WKScriptMessageHandlerWithReply>)m_handler.get() userContentController:m_controller.get() didReceiveScriptMessage:message.get() replyHandler:^(id result, NSString *errorMessage) {
                    if (!handler)
                        [NSException raise:NSInternalInconsistencyException format:@"replyHandler passed to userContentController:didReceiveScriptMessage:replyHandler: should not be called twice"];

                    if (errorMessage)
                        return handler(makeUnexpected(errorMessage));

                    auto extracted = WebKit::JavaScriptEvaluationResult::extract(result);
                    if (!extracted)
                        return handler(makeUnexpected("The result value passed back from the WKWebView API client was unable to be serialized"_s));
                    handler(WTF::move(*extracted));
                }];
                return;
            }

            [(id<WKScriptMessageHandler>)m_handler.get() userContentController:m_controller.get() didReceiveScriptMessage:message.get()];
            replyHandler(makeUnexpected(String()));
        }
    }

private:
    const RetainPtr<WKUserContentController> m_controller;
    const RetainPtr<id> m_handler;
    const RetainPtr<NSString> m_name;
    const bool m_supportsAsyncReply { false };
};

- (void)_addScriptMessageHandler:(WebKit::WebScriptMessageHandler&)scriptMessageHandler
{
    if (!protect(*_userContentControllerProxy)->addUserScriptMessageHandler(scriptMessageHandler))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", scriptMessageHandler.name().createNSString().get()];
}

- (void)addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, API::ContentWorld::pageContentWorldSingleton());
    [self _addScriptMessageHandler:handler.get()];
}

- (void)addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler contentWorld:(WKContentWorld *)world name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, Ref { *world->_contentWorld });
    [self _addScriptMessageHandler:handler.get()];
}

- (void)addScriptMessageHandlerWithReply:(id <WKScriptMessageHandlerWithReply>)scriptMessageHandler contentWorld:(WKContentWorld *)world name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, Ref { *world->_contentWorld });
    [self _addScriptMessageHandler:handler.get()];
}

- (void)removeScriptMessageHandlerForName:(NSString *)name
{
    protect(*_userContentControllerProxy)->removeUserMessageHandlerForName(name, API::ContentWorld::pageContentWorldSingleton());
}

- (void)removeScriptMessageHandlerForName:(NSString *)name contentWorld:(WKContentWorld *)contentWorld
{
    protect(*_userContentControllerProxy)->removeUserMessageHandlerForName(name, Ref { *contentWorld->_contentWorld });
}

- (void)removeAllScriptMessageHandlersFromContentWorld:(WKContentWorld *)contentWorld
{
    protect(*_userContentControllerProxy)->removeAllUserMessageHandlers(Ref { *contentWorld->_contentWorld });
}

- (void)removeAllScriptMessageHandlers
{
    protect(*_userContentControllerProxy)->removeAllUserMessageHandlers();
}

- (void)addBuffer:(WKJSScriptingBuffer *)buffer name:(NSString *)name contentWorld:(WKContentWorld *)world
{
    protect(*_userContentControllerProxy)->addJSBuffer(Ref { *buffer->_buffer }, Ref { *world->_contentWorld }, name);
}

- (void)removeBufferWithName:(NSString *)name contentWorld:(WKContentWorld *)world
{
    protect(*_userContentControllerProxy)->removeJSBuffer(Ref { *world->_contentWorld }, name);
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
    protect(*_userContentControllerProxy)->removeUserScript(Ref { *userScript->_userScript });
}

- (void)_removeAllUserScriptsAssociatedWithContentWorld:(WKContentWorld *)contentWorld
{
    protect(*_userContentControllerProxy)->removeAllUserScripts(Ref { *contentWorld->_contentWorld });
}

- (void)_addUserScriptImmediately:(WKUserScript *)userScript
{
    protect(*_userContentControllerProxy)->addUserScript(Ref { *userScript->_userScript }, WebKit::InjectUserScriptImmediately::Yes);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (void)_addUserContentFilter:(_WKUserContentFilter *)userContentFilter
#pragma clang diagnostic pop
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->addContentRuleList(Ref { *userContentFilter->_contentRuleList->_contentRuleList });
#endif
}

- (void)_addContentRuleList:(WKContentRuleList *)contentRuleList extensionBaseURL:(NSURL *)extensionBaseURL
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->addContentRuleList(Ref { *contentRuleList->_contentRuleList }, extensionBaseURL);
#endif
}

- (void)_removeUserContentFilter:(NSString *)userContentFilterName
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->removeContentRuleList(userContentFilterName);
#endif
}

- (void)_removeAllUserContentFilters
{
#if ENABLE(CONTENT_EXTENSIONS)
    protect(*_userContentControllerProxy)->removeAllContentRuleLists();
#endif
}

- (NSArray *)_userStyleSheets
{
    return wrapper(_userContentControllerProxy->userStyleSheets());
}

- (void)_addUserStyleSheet:(_WKUserStyleSheet *)userStyleSheet
{
    protect(*_userContentControllerProxy)->addUserStyleSheet(Ref { *userStyleSheet->_userStyleSheet });
}

- (void)_removeUserStyleSheet:(_WKUserStyleSheet *)userStyleSheet
{
    protect(*_userContentControllerProxy)->removeUserStyleSheet(Ref { *userStyleSheet->_userStyleSheet });
}

- (void)_removeAllUserStyleSheets
{
    protect(*_userContentControllerProxy)->removeAllUserStyleSheets();
}

- (void)_removeAllUserStyleSheetsAssociatedWithContentWorld:(WKContentWorld *)contentWorld
{
    protect(*_userContentControllerProxy)->removeAllUserStyleSheets(Ref { *contentWorld->_contentWorld });
}

- (void)_addBuffer:(_WKJSBuffer *)buffer contentWorld:(WKContentWorld *)world name:(NSString *)name
{
    [self addBuffer:buffer name:name contentWorld:world];
}

- (void)_removeBufferWithName:(NSString *)name contentWorld:(WKContentWorld *)world
{
    [self removeBufferWithName:name contentWorld:world];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (void)_addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    auto handler = WebKit::WebScriptMessageHandler::create(makeUnique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, Ref { *userContentWorld->_contentWorld->_contentWorld });
    if (!protect(*_userContentControllerProxy)->addUserScriptMessageHandler(handler.get()))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", name];
}

- (void)_addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name contentWorld:(WKContentWorld *)contentWorld
{
    [self _addScriptMessageHandler:scriptMessageHandler name:name userContentWorld:retainPtr(contentWorld._userContentWorld).get()];
}

- (void)_removeScriptMessageHandlerForName:(NSString *)name userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    protect(*_userContentControllerProxy)->removeUserMessageHandlerForName(name, Ref { *userContentWorld->_contentWorld->_contentWorld });
}

- (void)_removeAllScriptMessageHandlersAssociatedWithUserContentWorld:(_WKUserContentWorld *)userContentWorld
{
    protect(*_userContentControllerProxy)->removeAllUserMessageHandlers(Ref { *userContentWorld->_contentWorld->_contentWorld });
}
ALLOW_DEPRECATED_DECLARATIONS_END

@end
