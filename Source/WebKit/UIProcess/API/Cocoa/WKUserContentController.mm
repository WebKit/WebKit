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

#import "APISerializedScriptValue.h"
#import "APIUserContentWorld.h"
#import "InjectUserScriptImmediately.h"
#import "WKContentRuleListInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKNSArray.h"
#import "WKScriptMessageHandler.h"
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
public:
    ScriptMessageHandlerDelegate(WKUserContentController *controller, id <WKScriptMessageHandler> handler, NSString *name)
        : m_controller(controller)
        , m_handler(handler)
        , m_name(adoptNS([name copy]))
    {
    }
    
    void didPostMessage(WebKit::WebPageProxy& page, const WebKit::FrameInfoData& frameInfoData, WebCore::SerializedScriptValue& serializedScriptValue) override
    {
        @autoreleasepool {
            RetainPtr<WKFrameInfo> frameInfo = wrapper(API::FrameInfo::create(frameInfoData, &page));
            id body = API::SerializedScriptValue::deserialize(serializedScriptValue, 0);
            auto message = adoptNS([[WKScriptMessage alloc] _initWithBody:body webView:fromWebPageProxy(page) frameInfo:frameInfo.get() name:m_name.get()]);
        
            [m_handler userContentController:m_controller.get() didReceiveScriptMessage:message.get()];
        }
    }

private:
    RetainPtr<WKUserContentController> m_controller;
    RetainPtr<id <WKScriptMessageHandler>> m_handler;
    RetainPtr<NSString> m_name;
};

- (void)addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name
{
    auto handler = WebKit::WebScriptMessageHandler::create(std::make_unique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, API::UserContentWorld::normalWorld());
    if (!_userContentControllerProxy->addUserScriptMessageHandler(handler.get()))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", name];
}

- (void)removeScriptMessageHandlerForName:(NSString *)name
{
    _userContentControllerProxy->removeUserMessageHandlerForName(name, API::UserContentWorld::normalWorld());
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

- (void)_removeAllUserScriptsAssociatedWithUserContentWorld:(_WKUserContentWorld *)userContentWorld
{
    _userContentControllerProxy->removeAllUserScripts(*userContentWorld->_userContentWorld);
}

- (void)_addUserScriptImmediately:(WKUserScript *)userScript
{
    _userContentControllerProxy->addUserScript(*userScript->_userScript, WebKit::InjectUserScriptImmediately::Yes);
}

- (void)_addUserContentFilter:(_WKUserContentFilter *)userContentFilter
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->addContentRuleList(*userContentFilter->_contentRuleList->_contentRuleList);
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

- (void)_removeAllUserStyleSheetsAssociatedWithUserContentWorld:(_WKUserContentWorld *)userContentWorld
{
    _userContentControllerProxy->removeAllUserStyleSheets(*userContentWorld->_userContentWorld);
}

- (void)_addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    auto handler = WebKit::WebScriptMessageHandler::create(std::make_unique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name, *userContentWorld->_userContentWorld);
    if (!_userContentControllerProxy->addUserScriptMessageHandler(handler.get()))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", name];
}

- (void)_removeScriptMessageHandlerForName:(NSString *)name userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    _userContentControllerProxy->removeUserMessageHandlerForName(name, *userContentWorld->_userContentWorld);
}

- (void)_removeAllScriptMessageHandlersAssociatedWithUserContentWorld:(_WKUserContentWorld *)userContentWorld
{
    _userContentControllerProxy->removeAllUserMessageHandlers(*userContentWorld->_userContentWorld);
}

@end
