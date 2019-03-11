/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "WKContentRuleListInternal.h"
#import "WKContentRuleListStoreInternal.h"

#import "APIContentRuleListStore.h"
#import "NetworkCacheFilesystem.h"
#import "WKErrorInternal.h"
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>

static WKErrorCode toWKErrorCode(const std::error_code& error)
{
    ASSERT(error.category() == API::contentRuleListStoreErrorCategory());
    switch (static_cast<API::ContentRuleListStore::Error>(error.value())) {
    case API::ContentRuleListStore::Error::LookupFailed:
        return WKErrorContentRuleListStoreLookUpFailed;
    case API::ContentRuleListStore::Error::VersionMismatch:
        return WKErrorContentRuleListStoreVersionMismatch;
    case API::ContentRuleListStore::Error::CompileFailed:
        return WKErrorContentRuleListStoreCompileFailed;
    case API::ContentRuleListStore::Error::RemoveFailed:
        return WKErrorContentRuleListStoreRemoveFailed;
    }
    ASSERT_NOT_REACHED();
    return WKErrorUnknown;
}

@implementation WKContentRuleListStore

- (void)dealloc
{
    _contentRuleListStore->~ContentRuleListStore();

    [super dealloc];
}

+ (instancetype)defaultStore
{
    const bool legacyFilename = false;
    return wrapper(API::ContentRuleListStore::defaultStore(legacyFilename));
}

+ (instancetype)storeWithURL:(NSURL *)url
{
    const bool legacyFilename = false;
    return wrapper(API::ContentRuleListStore::storeWithPath(url.absoluteURL.fileSystemRepresentation, legacyFilename));
}

- (void)compileContentRuleListForIdentifier:(NSString *)identifier encodedContentRuleList:(NSString *)encodedContentRuleList completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler
{
    [self _compileContentRuleListForIdentifier:identifier encodedContentRuleList:[encodedContentRuleList retain] completionHandler:completionHandler];
}

- (void)lookUpContentRuleListForIdentifier:(NSString *)identifier completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler
{
    _contentRuleListStore->lookupContentRuleList(identifier, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        if (error) {
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Rule list lookup failed: %s", error.message().c_str()]};
            auto wkError = toWKErrorCode(error);
            ASSERT(wkError == WKErrorContentRuleListStoreLookUpFailed || wkError == WKErrorContentRuleListStoreVersionMismatch);
            return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:wkError userInfo:userInfo]);
        }

        completionHandler(wrapper(*contentRuleList), nil);
    });
}

- (void)getAvailableContentRuleListIdentifiers:(void (^)(NSArray<NSString *>*))completionHandler
{
    _contentRuleListStore->getAvailableContentRuleListIdentifiers([completionHandler = makeBlockPtr(completionHandler)](Vector<String> identifiers) {
        NSMutableArray<NSString *> *nsIdentifiers = [NSMutableArray arrayWithCapacity:identifiers.size()];
        for (const auto& identifier : identifiers)
            [nsIdentifiers addObject:identifier];
        completionHandler(nsIdentifiers);
    });
}

- (void)removeContentRuleListForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler
{
    _contentRuleListStore->removeContentRuleList(identifier, [completionHandler = makeBlockPtr(completionHandler)](std::error_code error) {
        if (error) {
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Rule list removal failed: %s", error.message().c_str()]};
            ASSERT(toWKErrorCode(error) == WKErrorContentRuleListStoreRemoveFailed);
            return completionHandler([NSError errorWithDomain:WKErrorDomain code:WKErrorContentRuleListStoreRemoveFailed userInfo:userInfo]);
        }

        completionHandler(nil);
    });
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_contentRuleListStore;
}

@end

@implementation WKContentRuleListStore (WKPrivate)

// For testing only.

+ (void)_registerPathAsUnsafeToMemoryMapForTesting:(NSString *)filename
{
    WebKit::NetworkCache::registerPathAsUnsafeToMemoryMapForTesting(filename);
}

- (void)_removeAllContentRuleLists
{
    _contentRuleListStore->synchronousRemoveAllContentRuleLists();
}

- (void)_invalidateContentRuleListVersionForIdentifier:(NSString *)identifier
{
    _contentRuleListStore->invalidateContentRuleListVersion(identifier);
}

- (void)_getContentRuleListSourceForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSString*))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);
    _contentRuleListStore->getContentRuleListSource(identifier, [handler](String source) {
        auto rawHandler = (void (^)(NSString *))handler.get();
        if (source.isNull())
            rawHandler(nil);
        else
            rawHandler(source);
    });
}

// NS_RELEASES_ARGUMENT to keep peak memory usage low.
- (void)_compileContentRuleListForIdentifier:(NSString *)identifier encodedContentRuleList:(NSString *) NS_RELEASES_ARGUMENT encodedContentRuleList completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler
{
    String json(encodedContentRuleList);
    [encodedContentRuleList release];
    encodedContentRuleList = nil;

    _contentRuleListStore->compileContentRuleList(identifier, WTFMove(json), [completionHandler = makeBlockPtr(completionHandler)](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        if (error) {
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Rule list compilation failed: %s", error.message().c_str()]};

            // error.value() could have a specific compiler error that is not equal to WKErrorContentRuleListStoreCompileFailed.
            // We want to use error.message, but here we want to only pass on CompileFailed with userInfo from the std::error_code.
            return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorContentRuleListStoreCompileFailed userInfo:userInfo]);
        }
        completionHandler(wrapper(*contentRuleList), nil);
    });
}

+ (instancetype)defaultStoreWithLegacyFilename
{
    const bool legacyFilename = true;
    return wrapper(API::ContentRuleListStore::defaultStore(legacyFilename));
}

+ (instancetype)storeWithURLAndLegacyFilename:(NSURL *)url
{
    const bool legacyFilename = true;
    return wrapper(API::ContentRuleListStore::storeWithPath(url.absoluteURL.fileSystemRepresentation, legacyFilename));
}

@end
