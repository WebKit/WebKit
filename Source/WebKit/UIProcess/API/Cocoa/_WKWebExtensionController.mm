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
#import "_WKWebExtensionControllerInternal.h"

#import "WebExtensionController.h"
#import "_WKWebExtensionContextInternal.h"
#import "_WKWebExtensionInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKWebExtensionController

#if ENABLE(WK_WEB_EXTENSIONS)

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionController>(self);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebExtensionController.class, self))
        return;

    _webExtensionController->~WebExtensionController();
}

- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext
{
    NSParameterAssert(extensionContext);

    return [self loadExtensionContext:extensionContext error:nullptr];
}

- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    NSParameterAssert(extensionContext);

    return _webExtensionController->load(extensionContext._webExtensionContext, outError);
}

- (BOOL)unloadExtensionContext:(_WKWebExtensionContext *)extensionContext
{
    NSParameterAssert(extensionContext);

    return [self unloadExtensionContext:extensionContext error:nullptr];
}

- (BOOL)unloadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    NSParameterAssert(extensionContext);

    return _webExtensionController->unload(extensionContext._webExtensionContext, outError);
}

- (_WKWebExtensionContext *)extensionContextForExtension:(_WKWebExtension *)extension
{
    NSParameterAssert(extension);

    if (auto extensionContext = _webExtensionController->extensionContext(extension._webExtension))
        return extensionContext->wrapper();
    return nil;
}

template<typename T>
static inline NSSet *toAPI(const T& inputSet)
{
    if (inputSet.isEmpty())
        return [NSSet set];

    NSMutableSet *result = [[NSMutableSet alloc] initWithCapacity:inputSet.size()];

    for (auto& entry : inputSet)
        [result addObject:entry->wrapper()];

    return [result copy];
}

- (NSSet<_WKWebExtension *> *)extensions
{
    auto extensions = _webExtensionController->extensions();
    return toAPI(extensions);
}

- (NSSet<_WKWebExtensionContext *> *)extensionContexts
{
    return toAPI(_webExtensionController->extensionContexts());
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionController;
}

- (WebKit::WebExtensionController&)_webExtensionController
{
    return *_webExtensionController;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext
{
    return NO;
}

- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)error
{
    return NO;
}

- (BOOL)unloadExtensionContext:(_WKWebExtensionContext *)extensionContext
{
    return NO;
}

- (BOOL)unloadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    return NO;
}

- (_WKWebExtensionContext *)extensionContextForExtension:(_WKWebExtension *)extension
{
    return nil;
}

- (NSSet<_WKWebExtension *> *)extensions
{
    return nil;
}

- (NSSet<_WKWebExtensionContext *> *)extensionContexts
{
    return nil;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
