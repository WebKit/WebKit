/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#import "_WKInspectorConfigurationInternal.h"

#import "WKProcessPoolInternal.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import "WebURLSchemeHandlerCocoa.h"
#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKInspectorConfiguration

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::InspectorConfiguration>(self);
    
    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKInspectorConfiguration.class, self))
        return;
    _configuration->API::InspectorConfiguration::~InspectorConfiguration();
    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_configuration;
}

- (void)setURLSchemeHandler:(id <WKURLSchemeHandler>)urlSchemeHandler forURLScheme:(NSString *)urlScheme
{
    _configuration->addURLSchemeHandler(WebKit::WebURLSchemeHandlerCocoa::create(urlSchemeHandler), urlScheme);
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (void)setProcessPool:(WKProcessPool *)processPool
{
    _configuration->setProcessPool(processPool ? processPool->_processPool.get() : nullptr);
}

- (WKProcessPool *)processPool
{
    return wrapper(_configuration->processPool());
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (void)applyToWebViewConfiguration:(WKWebViewConfiguration *)configuration
{
    for (auto pair : _configuration->urlSchemeHandlers()) {
        Ref handler = downcast<WebKit::WebURLSchemeHandlerCocoa>(pair.first.get());
        [configuration setURLSchemeHandler:handler->apiHandler() forURLScheme:pair.second.createNSString().get()];
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (auto* processPool = self.processPool)
        [configuration setProcessPool:processPool];
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (auto* groupIdentifier = self.groupIdentifier)
        [configuration _setGroupIdentifier:groupIdentifier];
}

- (id)copyWithZone:(NSZone *)zone
{
    _WKInspectorConfiguration *configuration = [(_WKInspectorConfiguration *)[[self class] allocWithZone:zone] init];

    for (auto pair : _configuration->urlSchemeHandlers()) {
        Ref handler = downcast<WebKit::WebURLSchemeHandlerCocoa>(pair.first.get());
        [configuration setURLSchemeHandler:handler->apiHandler() forURLScheme:pair.second.createNSString().get()];
    }

    if (auto* processPool = self.processPool)
        [configuration setProcessPool:processPool];

    if (auto* groupIdentifier = self.groupIdentifier)
        [configuration setGroupIdentifier:groupIdentifier];

    return configuration;
}

@end
