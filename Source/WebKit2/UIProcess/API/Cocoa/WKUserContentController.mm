/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

#import "APISerializedScriptValue.h"
#import "WKFrameInfoInternal.h"
#import "WKNSArray.h"
#import "WKScriptMessageHandler.h"
#import "WKScriptMessageInternal.h"
#import "WKUserScriptInternal.h"
#import "WKWebViewInternal.h"
#import "WebScriptMessageHandler.h"
#import "WebUserContentControllerProxy.h"
#import "_WKUserContentFilterInternal.h"
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

- (NSArray *)userScripts
{
    return wrapper(_userContentControllerProxy->userScripts());
}

- (void)addUserScript:(WKUserScript *)userScript
{
    _userContentControllerProxy->addUserScript(*userScript->_userScript);
}

- (void)removeAllUserScripts
{
    _userContentControllerProxy->removeAllUserScripts();
}

class ScriptMessageHandlerDelegate final : public WebKit::WebScriptMessageHandler::Client {
public:
    ScriptMessageHandlerDelegate(WKUserContentController *controller, id <WKScriptMessageHandler> handler, NSString *name)
        : m_controller(controller)
        , m_handler(handler)
        , m_name(adoptNS([name copy]))
    {
    }
    
    virtual void didPostMessage(WebKit::WebPageProxy& page, WebKit::WebFrameProxy& frame, const WebCore::SecurityOriginData& securityOriginData, WebCore::SerializedScriptValue& serializedScriptValue)
    {
        @autoreleasepool {
            RetainPtr<WKFrameInfo> frameInfo = wrapper(API::FrameInfo::create(frame, securityOriginData.securityOrigin()));
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
    RefPtr<WebKit::WebScriptMessageHandler> handler = WebKit::WebScriptMessageHandler::create(std::make_unique<ScriptMessageHandlerDelegate>(self, scriptMessageHandler, name), name);
    if (!_userContentControllerProxy->addUserScriptMessageHandler(handler.get()))
        [NSException raise:NSInvalidArgumentException format:@"Attempt to add script message handler with name '%@' when one already exists.", name];
}

- (void)removeScriptMessageHandlerForName:(NSString *)name
{
    _userContentControllerProxy->removeUserMessageHandlerForName(name);
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

- (void)_addUserContentFilter:(_WKUserContentFilter *)userContentFilter
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->addUserContentExtension(*userContentFilter->_userContentExtension);
#endif
}

- (void)_removeUserContentFilter:(NSString *)userContentFilterName
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->removeUserContentExtension(userContentFilterName);
#endif
}

- (void)_removeAllUserContentFilters
{
#if ENABLE(CONTENT_EXTENSIONS)
    _userContentControllerProxy->removeAllUserContentExtensions();
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

@end

#endif

