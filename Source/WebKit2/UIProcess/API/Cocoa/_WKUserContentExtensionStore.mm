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

#if WK_API_ENABLED

#import "WKErrorInternal.h"
#import "_WKUserContentFilterInternal.h"
#import <string>

@implementation _WKUserContentExtensionStore

- (void)dealloc
{
    _userContentExtensionStore->~UserContentExtensionStore();

    [super dealloc];
}

+ (instancetype)defaultStore
{
    return WebKit::wrapper(API::UserContentExtensionStore::defaultStore());
}

+ (instancetype)storeWithURL:(NSURL *)url
{
    Ref<API::UserContentExtensionStore> store = API::UserContentExtensionStore::storeWithPath(url.absoluteURL.fileSystemRepresentation);
    return WebKit::wrapper(store.leakRef());
}

- (void)compileContentExtensionForIdentifier:(NSString *)identifier encodedContentExtension:(NSString *)encodedContentExtension completionHandler:(void (^)(_WKUserContentFilter *, NSError *))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);

    String json(encodedContentExtension);
    [encodedContentExtension release];
    encodedContentExtension = nil;

    _userContentExtensionStore->compileContentExtension(identifier, WTFMove(json), [handler](RefPtr<API::UserContentExtension> contentExtension, std::error_code error) {
        if (error) {
            auto rawHandler = (void (^)(_WKUserContentFilter *, NSError *))handler.get();
            
            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Extension compilation failed: %s", error.message().c_str()]};
            rawHandler(nil, [NSError errorWithDomain:@"ContentExtensionsDomain" code:error.value() userInfo:userInfo]);
            return;
        }

        auto rawHandler = (void (^)(_WKUserContentFilter *, NSError *))handler.get();
        rawHandler(WebKit::wrapper(*contentExtension.get()), nil);
    });
}

- (void)lookupContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(_WKUserContentFilter *, NSError *))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);

    _userContentExtensionStore->lookupContentExtension(identifier, [handler](RefPtr<API::UserContentExtension> contentExtension, std::error_code error) {
        if (error) {
            auto rawHandler = (void (^)(_WKUserContentFilter *, NSError *))handler.get();

            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Extension lookup failed: %s", error.message().c_str()]};
            rawHandler(nil, [NSError errorWithDomain:@"ContentExtensionsDomain" code:error.value() userInfo:userInfo]);
            return;
        }

        auto rawHandler = (void (^)(_WKUserContentFilter *, NSError *))handler.get();
        rawHandler(WebKit::wrapper(*contentExtension.get()), nil);
    });
}

- (void)removeContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler
{
    auto handler = adoptNS([completionHandler copy]);

    _userContentExtensionStore->removeContentExtension(identifier, [handler](std::error_code error) {
        if (error) {
            auto rawHandler = (void (^)(NSError *))handler.get();

            auto userInfo = @{NSHelpAnchorErrorKey: [NSString stringWithFormat:@"Extension removal failed: %s", error.message().c_str()]};
            rawHandler([NSError errorWithDomain:@"ContentExtensionsDomain" code:error.value() userInfo:userInfo]);
            return;
        }

        auto rawHandler = (void (^)(NSError *))handler.get();
        rawHandler(nil);
    });
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_userContentExtensionStore;
}

@end

@implementation _WKUserContentExtensionStore (WKPrivate)

// For testing only.

- (void)_removeAllContentExtensions
{
    _userContentExtensionStore->synchronousRemoveAllContentExtensions();
}

@end

#endif // WK_API_ENABLED
