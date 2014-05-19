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

#import "WKUserScriptInternal.h"
#import "WebUserContentControllerProxy.h"
#import "_WKScriptWorld.h"
#import <WebCore/UserScript.h>
#import <wtf/text/StringBuilder.h>

@implementation WKUserContentController {
    RetainPtr<NSMutableArray> _userScripts;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _userContentControllerProxy = WebKit::WebUserContentControllerProxy::create();
    _userScripts = adoptNS([[NSMutableArray alloc] init]);

    return self;
}

- (NSArray *)userScripts
{
    return _userScripts.get();
}

static WebCore::UserScriptInjectionTime toWebCoreUserScriptInjectionTime(WKUserScriptInjectionTime injectionTime)
{
    switch (injectionTime) {
    case WKUserScriptInjectionTimeAtDocumentStart:
        return WebCore::InjectAtDocumentStart;

    case WKUserScriptInjectionTimeAtDocumentEnd:
        return WebCore::InjectAtDocumentEnd;
    }

    ASSERT_NOT_REACHED();
    return WebCore::InjectAtDocumentEnd;
}

- (void)addUserScript:(WKUserScript *)userScript
{
    [_userScripts addObject:userScript];

    StringBuilder urlStringBuilder;
    urlStringBuilder.append("user-script:");
    urlStringBuilder.appendNumber([_userScripts count]);

    WebCore::URL url { WebCore::URL { }, urlStringBuilder.toString() };
    _userContentControllerProxy->addUserScript(WebCore::UserScript { userScript->_source.get(), url, { }, { }, toWebCoreUserScriptInjectionTime(userScript->_injectionTime), userScript->_forMainFrameOnly ? WebCore::InjectInTopFrameOnly : WebCore::InjectInAllFrames });
}

- (void)removeAllUserScripts
{
    [_userScripts removeAllObjects];

    _userContentControllerProxy->removeAllUserScripts();
}

- (void)addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name
{
    [self _addScriptMessageHandler:scriptMessageHandler name:name world:[_WKScriptWorld defaultWorld]];
}

- (void)removeScriptMessageHandlerForName:(NSString *)name
{
    [self _removeScriptMessageHandlerForName:name world:[_WKScriptWorld defaultWorld]];
}

@end

@implementation WKUserContentController (WKPrivate)

- (void)_addScriptMessageHandler:(id <WKScriptMessageHandler>)scriptMessageHandler name:(NSString *)name world:(_WKScriptWorld *)world
{
    // FIXME: Implement.
}

- (void)_removeScriptMessageHandlerForName:(NSString *)name world:(_WKScriptWorld *)world
{
    // FIXME: Implement.
}

@end

#endif

