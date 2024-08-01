/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionControllerInternal.h"

#import "WKWebExtensionContextInternal.h"
#import "WKWebExtensionControllerConfigurationInternal.h"
#import "WKWebExtensionDataRecordInternal.h"
#import "WKWebExtensionDataTypeInternal.h"
#import "WKWebExtensionInternal.h"
#import "WebExtensionContext.h"
#import "WebExtensionController.h"
#import "WebExtensionDataType.h"
#import <WebCore/EventRegion.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>

@implementation WKWebExtensionController

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtensionController, WebExtensionController, _webExtensionController);

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionController>(self, WebKit::WebExtensionControllerConfiguration::createDefault());

    return self;
}

- (instancetype)initWithConfiguration:(WKWebExtensionControllerConfiguration *)configuration
{
    NSParameterAssert([configuration isKindOfClass:WKWebExtensionControllerConfiguration.class]);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionController>(self, configuration._webExtensionControllerConfiguration.copy());

    return self;
}

- (WKWebExtensionControllerConfiguration *)configuration
{
    return _webExtensionController->configuration().copy()->wrapper();
}

- (BOOL)loadExtensionContext:(WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    NSParameterAssert([extensionContext isKindOfClass:WKWebExtensionContext.class]);

    return _webExtensionController->load(extensionContext._webExtensionContext, outError);
}

- (BOOL)unloadExtensionContext:(WKWebExtensionContext *)extensionContext error:(NSError **)outError
{
    NSParameterAssert([extensionContext isKindOfClass:WKWebExtensionContext.class]);

    return _webExtensionController->unload(extensionContext._webExtensionContext, outError);
}

- (WKWebExtensionContext *)extensionContextForExtension:(WKWebExtension *)extension
{
    NSParameterAssert([extension isKindOfClass:WKWebExtension.class]);

    if (auto extensionContext = _webExtensionController->extensionContext(extension._webExtension))
        return extensionContext->wrapper();
    return nil;
}

- (WKWebExtensionContext *)extensionContextForURL:(NSURL *)url
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

- (NSSet<WKWebExtension *> *)extensions
{
    return toAPI(_webExtensionController->extensions());
}

- (NSSet<WKWebExtensionContext *> *)extensionContexts
{
    return toAPI(_webExtensionController->extensionContexts());
}

+ (NSSet<WKWebExtensionDataType> *)allExtensionDataTypes
{
    return [NSSet setWithObjects:WKWebExtensionDataTypeLocal, WKWebExtensionDataTypeSession, WKWebExtensionDataTypeSync, nil];
}

- (void)fetchDataRecordsOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes completionHandler:(void (^)(NSArray<WKWebExtensionDataRecord *> *))completionHandler
{
    NSParameterAssert([dataTypes isKindOfClass:NSSet.class]);
    NSParameterAssert(completionHandler);

    _webExtensionController->getDataRecords(WebKit::toWebExtensionDataTypes(dataTypes), [completionHandler = makeBlockPtr(completionHandler)](Vector<Ref<WebKit::WebExtensionDataRecord>> records) {
        completionHandler(toAPI(records));
    });
}

- (void)fetchDataRecordOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(WKWebExtensionDataRecord *))completionHandler
{
    NSParameterAssert([dataTypes isKindOfClass:NSSet.class]);
    NSParameterAssert([extensionContext isKindOfClass:WKWebExtensionContext.class]);
    NSParameterAssert(completionHandler);

    _webExtensionController->getDataRecord(WebKit::toWebExtensionDataTypes(dataTypes), extensionContext._webExtensionContext, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<WebKit::WebExtensionDataRecord> record) {
        if (record)
            completionHandler(record->wrapper());
        else
            completionHandler(nil);
    });
}

- (void)removeDataOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes forDataRecords:(NSArray<WKWebExtensionDataRecord *> *)dataRecords completionHandler:(void (^)())completionHandler
{
    NSParameterAssert([dataTypes isKindOfClass:NSSet.class]);
    NSParameterAssert([dataRecords isKindOfClass:NSArray.class]);
    NSParameterAssert(completionHandler);

    _webExtensionController->removeData(WebKit::toWebExtensionDataTypes(dataTypes), WebKit::toWebExtensionDataRecords(dataRecords), [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)didOpenWindow:(id<WKWebExtensionWindow>)newWindow
{
    NSParameterAssert([newWindow conformsToProtocol:@protocol(WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didOpenWindow:newWindow];
}

- (void)didCloseWindow:(id<WKWebExtensionWindow>)closedWindow
{
    NSParameterAssert([closedWindow conformsToProtocol:@protocol(WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didCloseWindow:closedWindow];
}

- (void)didFocusWindow:(id<WKWebExtensionWindow>)focusedWindow
{
    if (focusedWindow)
        NSParameterAssert([focusedWindow conformsToProtocol:@protocol(WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didFocusWindow:focusedWindow];
}

- (void)didOpenTab:(id<WKWebExtensionTab>)newTab
{
    NSParameterAssert([newTab conformsToProtocol:@protocol(WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didOpenTab:newTab];
}

- (void)didCloseTab:(id<WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
    NSParameterAssert([closedTab conformsToProtocol:@protocol(WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didCloseTab:closedTab windowIsClosing:windowIsClosing];
}

- (void)didActivateTab:(id<WKWebExtensionTab>)activatedTab previousActiveTab:(nullable id<WKWebExtensionTab>)previousTab
{
    NSParameterAssert([activatedTab conformsToProtocol:@protocol(WKWebExtensionTab)]);
    if (previousTab)
        NSParameterAssert([previousTab conformsToProtocol:@protocol(WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didActivateTab:activatedTab previousActiveTab:previousTab];
}

- (void)didSelectTabs:(NSSet<id<WKWebExtensionTab>> *)selectedTabs
{
    NSParameterAssert([selectedTabs isKindOfClass:NSSet.class]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didSelectTabs:selectedTabs];
}

- (void)didDeselectTabs:(NSSet<id<WKWebExtensionTab>> *)deselectedTabs
{
    NSParameterAssert([deselectedTabs isKindOfClass:NSSet.class]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didDeselectTabs:deselectedTabs];
}

- (void)didMoveTab:(id<WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<WKWebExtensionWindow>)oldWindow
{
    NSParameterAssert([movedTab conformsToProtocol:@protocol(WKWebExtensionTab)]);
    if (oldWindow)
        NSParameterAssert([oldWindow conformsToProtocol:@protocol(WKWebExtensionWindow)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didMoveTab:movedTab fromIndex:index inWindow:oldWindow];
}

- (void)didReplaceTab:(id<WKWebExtensionTab>)oldTab withTab:(id<WKWebExtensionTab>)newTab
{
    NSParameterAssert([oldTab conformsToProtocol:@protocol(WKWebExtensionTab)]);
    NSParameterAssert([newTab conformsToProtocol:@protocol(WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didReplaceTab:oldTab withTab:newTab];
}

- (void)didChangeTabProperties:(WKWebExtensionTabChangedProperties)properties forTab:(id<WKWebExtensionTab>)changedTab
{
    NSParameterAssert([changedTab conformsToProtocol:@protocol(WKWebExtensionTab)]);

    for (auto& context : _webExtensionController->extensionContexts())
        [context->wrapper() didChangeTabProperties:properties forTab:changedTab];
}

- (BOOL)_inTestingMode
{
    return _webExtensionController->inTestingMode();
}

- (void)_setTestingMode:(BOOL)testingMode
{
    _webExtensionController->setTestingMode(testingMode);
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

- (instancetype)initWithConfiguration:(WKWebExtensionControllerConfiguration *)configuration
{
    return nil;
}

- (WKWebExtensionControllerConfiguration *)configuration
{
    return nil;
}

- (BOOL)loadExtensionContext:(WKWebExtensionContext *)extensionContext error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return NO;
}

- (BOOL)unloadExtensionContext:(WKWebExtensionContext *)extensionContext error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return NO;
}

- (WKWebExtensionContext *)extensionContextForExtension:(WKWebExtension *)extension
{
    return nil;
}

- (WKWebExtensionContext *)extensionContextForURL:(NSURL *)url
{
    return nil;
}

- (NSSet<WKWebExtension *> *)extensions
{
    return nil;
}

- (NSSet<WKWebExtensionContext *> *)extensionContexts
{
    return nil;
}

+ (NSSet<WKWebExtensionDataType> *)allExtensionDataTypes
{
    return nil;
}

- (void)fetchDataRecordsOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes completionHandler:(void (^)(NSArray<WKWebExtensionDataRecord *> *))completionHandler
{
    completionHandler(@[ ]);
}

- (void)fetchDataRecordOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(WKWebExtensionDataRecord *))completionHandler
{
    completionHandler(nil);
}

- (void)removeDataOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes forDataRecords:(NSArray<WKWebExtensionDataRecord *> *)dataRecords completionHandler:(void (^)())completionHandler
{
    completionHandler();
}

- (void)didOpenWindow:(id<WKWebExtensionWindow>)newWindow
{
}

- (void)didCloseWindow:(id<WKWebExtensionWindow>)closedWindow
{
}

- (void)didFocusWindow:(id<WKWebExtensionWindow>)focusedWindow
{
}

- (void)didOpenTab:(id<WKWebExtensionTab>)newTab
{
}

- (void)didCloseTab:(id<WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
}

- (void)didActivateTab:(id<WKWebExtensionTab>)activatedTab previousActiveTab:(nullable id<WKWebExtensionTab>)previousTab
{
}

- (void)didSelectTabs:(NSSet<id<WKWebExtensionTab>> *)selectedTabs
{
}

- (void)didDeselectTabs:(NSSet<id<WKWebExtensionTab>> *)deselectedTabs
{
}

- (void)didMoveTab:(id<WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<WKWebExtensionWindow>)oldWindow
{
}

- (void)didReplaceTab:(id<WKWebExtensionTab>)oldTab withTab:(id<WKWebExtensionTab>)newTab
{
}

- (void)didChangeTabProperties:(WKWebExtensionTabChangedProperties)properties forTab:(id<WKWebExtensionTab>)changedTab
{
}

- (BOOL)_inTestingMode
{
    return NO;
}

- (void)_setTestingMode:(BOOL)testingMode
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
