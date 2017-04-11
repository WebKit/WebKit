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
#import "WKContentExtensionInternal.h"
#import "WKContentExtensionStoreInternal.h"

#if WK_API_ENABLED

#import "APIContentExtensionStore.h"
#import "WKErrorInternal.h"
#import <wtf/BlockPtr.h>

static WKErrorCode toWKErrorCode(const std::error_code& error)
{
    ASSERT(error.category() == API::contentExtensionStoreErrorCategory());
    switch (static_cast<API::ContentExtensionStore::Error>(error.value())) {
    case API::ContentExtensionStore::Error::LookupFailed:
        return WKErrorContentExtensionStoreLookUpFailed;
    case API::ContentExtensionStore::Error::VersionMismatch:
        return WKErrorContentExtensionStoreVersionMismatch;
    case API::ContentExtensionStore::Error::CompileFailed:
        return WKErrorContentExtensionStoreCompileFailed;
    case API::ContentExtensionStore::Error::RemoveFailed:
        return WKErrorContentExtensionStoreRemoveFailed;
    }
    ASSERT_NOT_REACHED();
    return WKErrorUnknown;
}

@implementation WKContentExtensionStore

- (void)dealloc
{
    _contentExtensionStore->~ContentExtensionStore();

    [super dealloc];
}

+ (instancetype)defaultStore
{
    return WebKit::wrapper(API::ContentExtensionStore::defaultStore());
}

+ (instancetype)storeWithURL:(NSURL *)url
{
    Ref<API::ContentExtensionStore> store = API::ContentExtensionStore::storeWithPath(url.absoluteURL.fileSystemRepresentation);
    return WebKit::wrapper(store.leakRef());
}

- (void)compileContentExtensionForIdentifier:(NSString *)identifier encodedContentExtension:(NSString *)encodedContentExtension completionHandler:(void (^)(WKContentExtension *, NSError *))completionHandler
{
    [self _compileContentExtensionForIdentifier:identifier encodedContentExtension:encodedContentExtension completionHandler:completionHandler releasesArgument:NO];
}

- (void)_compileContentExtensionForIdentifier:(NSString *)identifier encodedContentExtension:(NSString *)encodedContentExtension completionHandler:(void (^)(WKContentExtension *, NSError *))completionHandler releasesArgument:(BOOL)releasesArgument
{
    String json(encodedContentExtension);
    if (releasesArgument) {
        [encodedContentExtension release];
        encodedContentExtension = nil;
    }

    _contentExtensionStore->compileContentExtension(identifier, WTFMove(json), [completionHandler = makeBlockPtr(completionHandler)](RefPtr<API::ContentExtension> contentExtension, std::error_code error) {
        if (error) {
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Extension compilation failed: %s", error.message().c_str()]};

            // error.value() could have a specific compiler error that is not equal to WKErrorContentExtensionStoreCompileFailed.
            // We want to use error.message, but here we want to only pass on CompileFailed with userInfo from the std::error_code.
            return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorContentExtensionStoreCompileFailed userInfo:userInfo]);
        }
        completionHandler(WebKit::wrapper(*contentExtension.get()), nil);
    });
}

- (void)lookUpContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(WKContentExtension *, NSError *))completionHandler
{
    _contentExtensionStore->lookupContentExtension(identifier, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<API::ContentExtension> contentExtension, std::error_code error) {
        if (error) {
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Extension lookup failed: %s", error.message().c_str()]};
            auto wkError = toWKErrorCode(error);
            ASSERT(wkError == WKErrorContentExtensionStoreLookUpFailed || wkError == WKErrorContentExtensionStoreVersionMismatch);
            return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:wkError userInfo:userInfo]);
        }

        completionHandler(WebKit::wrapper(*contentExtension.get()), nil);
    });
}

- (void)getAvailableContentExtensionIdentifiers:(void (^)(NSArray<NSString *>*))completionHandler
{
    _contentExtensionStore->getAvailableContentExtensionIdentifiers([completionHandler = makeBlockPtr(completionHandler)](Vector<String> identifiers) {
        NSMutableArray<NSString *> *nsIdentifiers = [NSMutableArray arrayWithCapacity:identifiers.size()];
        for (const auto& identifier : identifiers)
            [nsIdentifiers addObject:identifier];
        completionHandler(nsIdentifiers);
    });
}

- (void)removeContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler
{
    _contentExtensionStore->removeContentExtension(identifier, [completionHandler = makeBlockPtr(completionHandler)](std::error_code error) {
        if (error) {
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Extension removal failed: %s", error.message().c_str()]};
            ASSERT(toWKErrorCode(error) == WKErrorContentExtensionStoreRemoveFailed);
            return completionHandler([NSError errorWithDomain:WKErrorDomain code:WKErrorContentExtensionStoreRemoveFailed userInfo:userInfo]);
        }

        completionHandler(nil);
    });
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_contentExtensionStore;
}

@end

@implementation WKContentExtensionStore (WKPrivate)

// For testing only.

- (void)_removeAllContentExtensions
{
    _contentExtensionStore->synchronousRemoveAllContentExtensions();
}

- (void)_invalidateContentExtensionVersionForIdentifier:(NSString *)identifier
{
    _contentExtensionStore->invalidateContentExtensionVersion(identifier);
}

- (void)_getContentExtensionSourceForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSString*))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);
    _contentExtensionStore->getContentExtensionSource(identifier, [handler](String source) {
        auto rawHandler = (void (^)(NSString *))handler.get();
        if (source.isNull())
            rawHandler(nil);
        else
            rawHandler(source);
    });
}

// NS_RELEASES_ARGUMENT to keep peak memory usage low.

- (void)_compileContentExtensionForIdentifier:(NSString *)identifier encodedContentExtension:(NSString *)encodedContentExtension completionHandler:(void (^)(WKContentExtension *, NSError *))completionHandler
{
    [self _compileContentExtensionForIdentifier:identifier encodedContentExtension:encodedContentExtension completionHandler:completionHandler releasesArgument:YES];
}

@end

#endif // WK_API_ENABLED
