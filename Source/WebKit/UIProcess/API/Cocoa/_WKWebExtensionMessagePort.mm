/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "_WKWebExtensionMessagePortInternal.h"

#import "WebExtensionMessagePort.h"
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>

NSErrorDomain const _WKWebExtensionMessagePortErrorDomain = @"_WKWebExtensionMessagePortErrorDomain";

@implementation _WKWebExtensionMessagePort

#if ENABLE(WK_WEB_EXTENSIONS)

- (void)dealloc
{
    ASSERT(isMainRunLoop());

    _webExtensionMessagePort->~WebExtensionMessagePort();
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<_WKWebExtensionMessagePort>(object);
    if (!other)
        return NO;

    return *_webExtensionMessagePort == *other->_webExtensionMessagePort;
}

- (NSString *)applicationIdentifier
{
    if (auto& applicationIdentifier = _webExtensionMessagePort->applicationIdentifier(); !applicationIdentifier.isNull())
        return applicationIdentifier;
    return nil;
}

- (BOOL)isDisconnected
{
    return _webExtensionMessagePort->isDisconnected();
}

- (void)sendMessage:(id)message completionHandler:(void (^)(BOOL success, NSError *))completionHandler
{
    NSParameterAssert(message);

    if (!completionHandler)
        completionHandler = ^(BOOL, NSError *) { };

    _webExtensionMessagePort->sendMessage(message, [completionHandler = makeBlockPtr(completionHandler)] (WebKit::WebExtensionMessagePort::Error error) {
        if (error) {
            completionHandler(NO, toAPI(error.value()));
            return;
        }

        completionHandler(YES, nil);
    });
}

- (void)disconnectWithError:(NSError *)error
{
    _webExtensionMessagePort->disconnect(WebKit::toWebExtensionMessagePortError(error));
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionMessagePort;
}

- (WebKit::WebExtensionMessagePort&)_webExtensionMessagePort
{
    return *_webExtensionMessagePort;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (NSString *)applicationIdentifier
{
    return nil;
}

- (BOOL)isDisconnected
{
    return NO;
}

- (void)sendMessage:(id)message completionHandler:(void (^)(BOOL success, NSError *))completionHandler
{
}

- (void)disconnectWithError:(NSError *)error
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
