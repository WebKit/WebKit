/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "_WKUserContentExtensionStoreInternal.h"

#import "WKContentRuleListStoreInternal.h"
#import "WKContentRuleListStorePrivate.h"
#import "WKErrorInternal.h"
#import "_WKUserContentExtensionStorePrivate.h"
#import "_WKUserContentFilterInternal.h"
#import "_WKUserContentFilterPrivate.h"
#import <string>

NSString * const _WKUserContentExtensionsDomain = @"WKErrorDomain";

static NSError *toUserContentRuleListStoreError(const NSError *error)
{
    if (!error)
        return nil;

    ASSERT(error.domain == WKErrorDomain);
    switch (error.code) {
    case WKErrorContentRuleListStoreLookUpFailed:
        return [NSError errorWithDomain:_WKUserContentExtensionsDomain code:_WKUserContentExtensionStoreErrorLookupFailed userInfo:error.userInfo];
    case WKErrorContentRuleListStoreVersionMismatch:
        return [NSError errorWithDomain:_WKUserContentExtensionsDomain code:_WKUserContentExtensionStoreErrorVersionMismatch userInfo:error.userInfo];
    case WKErrorContentRuleListStoreCompileFailed:
        return [NSError errorWithDomain:_WKUserContentExtensionsDomain code:_WKUserContentExtensionStoreErrorCompileFailed userInfo:error.userInfo];
    case WKErrorContentRuleListStoreRemoveFailed:
        return [NSError errorWithDomain:_WKUserContentExtensionsDomain code:_WKUserContentExtensionStoreErrorRemoveFailed userInfo:error.userInfo];
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

@implementation _WKUserContentExtensionStore

+ (instancetype)defaultStore
{
    return [[[_WKUserContentExtensionStore alloc] _initWithWKContentRuleListStore:[WKContentRuleListStore defaultStoreWithLegacyFilename]] autorelease];
}

+ (instancetype)storeWithURL:(NSURL *)url
{
    return [[[_WKUserContentExtensionStore alloc] _initWithWKContentRuleListStore:[WKContentRuleListStore storeWithURLAndLegacyFilename:url]] autorelease];
}

- (void)compileContentExtensionForIdentifier:(NSString *)identifier encodedContentExtension:(NSString *)encodedContentRuleList completionHandler:(void (^)(_WKUserContentFilter *, NSError *))completionHandler
{
    [_contentRuleListStore _compileContentRuleListForIdentifier:identifier encodedContentRuleList:encodedContentRuleList completionHandler:^(WKContentRuleList *contentRuleList, NSError *error) {
        _WKUserContentFilter *contentFilter = contentRuleList ? [[[_WKUserContentFilter alloc] _initWithWKContentRuleList:contentRuleList] autorelease] : nil;
        completionHandler(contentFilter, toUserContentRuleListStoreError(error));
    }];
}

- (void)lookupContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(_WKUserContentFilter *, NSError *))completionHandler
{
    [_contentRuleListStore lookUpContentRuleListForIdentifier:identifier completionHandler:^(WKContentRuleList *contentRuleList, NSError *error) {
        _WKUserContentFilter *contentFilter = contentRuleList ? [[[_WKUserContentFilter alloc] _initWithWKContentRuleList:contentRuleList] autorelease] : nil;
        completionHandler(contentFilter, toUserContentRuleListStoreError(error));
    }];
}

- (void)removeContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler
{
    [_contentRuleListStore removeContentRuleListForIdentifier:identifier completionHandler:^(NSError *error) {
        completionHandler(toUserContentRuleListStoreError(error));
    }];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return [_contentRuleListStore _apiObject];
}

@end

@implementation _WKUserContentExtensionStore (WKPrivate)

// For testing only.

- (void)_removeAllContentExtensions
{
    [_contentRuleListStore _removeAllContentRuleLists];
}

- (void)_invalidateContentExtensionVersionForIdentifier:(NSString *)identifier
{
    [_contentRuleListStore _invalidateContentRuleListVersionForIdentifier:identifier];
}

- (id)_initWithWKContentRuleListStore:(WKContentRuleListStore*)contentRuleListStore
{
    self = [super init];
    if (!self)
        return nil;
    
    _contentRuleListStore = contentRuleListStore;
    
    return self;
}

- (WKContentRuleListStore *)_contentRuleListStore
{
    return _contentRuleListStore.get();
}

@end
