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
#import "_WKWebExtensionControllerConfigurationInternal.h"
#import "_WKWebExtensionInternal.h"
#import <WebCore/EventRegion.h>

@implementation _WKWebExtensionController

#if ENABLE(WK_WEB_EXTENSIONS)

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionController>(self, WebKit::WebExtensionControllerConfiguration::createDefault());

    return self;
}

- (instancetype)initWithConfiguration:(_WKWebExtensionControllerConfiguration *)configuration
{
    NSParameterAssert([configuration isKindOfClass:_WKWebExtensionControllerConfiguration.class]);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionController>(self, configuration._webExtensionControllerConfiguration.copy());

    return self;
}

- (void)dealloc
{
    ASSERT(isMainRunLoop());

    _webExtensionController->~WebExtensionController();
}

- (_WKWebExtensionControllerConfiguration *)configuration
{
    return _webExtensionController->configuration().copy()->wrapper();
}

- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    NSParameterAssert([extensionContext isKindOfClass:_WKWebExtensionContext.class]);

    return _webExtensionController->load(extensionContext._webExtensionContext, outError);
}

- (BOOL)unloadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    NSParameterAssert([extensionContext isKindOfClass:_WKWebExtensionContext.class]);

    return _webExtensionController->unload(extensionContext._webExtensionContext, outError);
}

- (_WKWebExtensionContext *)extensionContextForExtension:(_WKWebExtension *)extension
{
    NSParameterAssert([extension isKindOfClass:_WKWebExtension.class]);

    if (auto extensionContext = _webExtensionController->extensionContext(extension._webExtension))
        return extensionContext->wrapper();
    return nil;
}

- (_WKWebExtensionContext *)extensionContextForURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    if (auto extensionContext = _webExtensionController->extensionContext(url))
        return extensionContext->wrapper();
    return nil;
}

template<typename T>
static inline NSSet *toAPI(const HashSet<Ref<T>>& inputSet)
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
    return toAPI(_webExtensionController->extensions());
}

- (NSSet<_WKWebExtensionContext *> *)extensionContexts
{
    return toAPI(_webExtensionController->extensionContexts());
}

- (void)didOpenWindow:(id<_WKWebExtensionWindow>)newWindow
{
    NSParameterAssert([newWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didOpenWindow:newWindow];
}

- (void)didCloseWindow:(id<_WKWebExtensionWindow>)closedWindow
{
    NSParameterAssert([closedWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didCloseWindow:closedWindow];
}

- (void)didFocusWindow:(id<_WKWebExtensionWindow>)focusedWindow
{
    if (focusedWindow)
        NSParameterAssert([focusedWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didFocusWindow:focusedWindow];
}

- (void)didOpenTab:(id<_WKWebExtensionTab>)newTab
{
    NSParameterAssert([newTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didOpenTab:newTab];
}

- (void)didCloseTab:(id<_WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
    NSParameterAssert([closedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didCloseTab:closedTab windowIsClosing:windowIsClosing];
}

- (void)didActivateTab:(id<_WKWebExtensionTab>)activatedTab previousActiveTab:(nullable id<_WKWebExtensionTab>)previousTab
{
    NSParameterAssert([activatedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
    if (previousTab)
        NSParameterAssert([previousTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didActivateTab:activatedTab previousActiveTab:previousTab];
}

- (void)didSelectTabs:(NSSet<id<_WKWebExtensionTab>> *)selectedTabs
{
    NSParameterAssert([selectedTabs isKindOfClass:NSSet.class]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didSelectTabs:selectedTabs];
}

- (void)didDeselectTabs:(NSSet<id<_WKWebExtensionTab>> *)deselectedTabs
{
    NSParameterAssert([deselectedTabs isKindOfClass:NSSet.class]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didDeselectTabs:deselectedTabs];
}

- (void)didMoveTab:(id<_WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<_WKWebExtensionWindow>)oldWindow
{
    NSParameterAssert([movedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
    if (oldWindow)
        NSParameterAssert([oldWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didMoveTab:movedTab fromIndex:index inWindow:oldWindow];
}

- (void)didReplaceTab:(id<_WKWebExtensionTab>)oldTab withTab:(id<_WKWebExtensionTab>)newTab
{
    NSParameterAssert([oldTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
    NSParameterAssert([newTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didReplaceTab:oldTab withTab:newTab];
}

- (void)didChangeTabProperties:(_WKWebExtensionTabChangedProperties)properties forTab:(id<_WKWebExtensionTab>)changedTab
{
    NSParameterAssert([changedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didChangeTabProperties:properties forTab:changedTab];
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

- (instancetype)init
{
    return nil;
}

- (instancetype)initWithConfiguration:(_WKWebExtensionControllerConfiguration *)configuration
{
    return nil;
}

- (_WKWebExtensionControllerConfiguration *)configuration
{
    return nil;
}

- (BOOL)loadExtensionContext:(_WKWebExtensionContext *)extensionContext error:(NSError **)error
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

- (_WKWebExtensionContext *)extensionContextForURL:(NSURL *)url
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

- (void)didOpenWindow:(id<_WKWebExtensionWindow>)newWindow
{
}

- (void)didCloseWindow:(id<_WKWebExtensionWindow>)closedWindow
{
}

- (void)didFocusWindow:(id<_WKWebExtensionWindow>)focusedWindow
{
}

- (void)didOpenTab:(id<_WKWebExtensionTab>)newTab
{
}

- (void)didCloseTab:(id<_WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
}

- (void)didActivateTab:(id<_WKWebExtensionTab>)activatedTab previousActiveTab:(nullable id<_WKWebExtensionTab>)previousTab
{
}

- (void)didSelectTabs:(NSSet<id<_WKWebExtensionTab>> *)selectedTabs
{
}

- (void)didDeselectTabs:(NSSet<id<_WKWebExtensionTab>> *)deselectedTabs
{
}

- (void)didMoveTab:(id<_WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<_WKWebExtensionWindow>)oldWindow
{
}

- (void)didReplaceTab:(id<_WKWebExtensionTab>)oldTab withTab:(id<_WKWebExtensionTab>)newTab
{
}

- (void)didChangeTabProperties:(_WKWebExtensionTabChangedProperties)properties forTab:(id<_WKWebExtensionTab>)changedTab
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
