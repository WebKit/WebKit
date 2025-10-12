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
#import "NetworkCacheFileSystem.h"
#import "WKErrorInternal.h"
#import "WebPrivacyHelpers.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(CONTENT_EXTENSIONS)
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
#endif

@implementation WKContentRuleListStore

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKContentRuleListStore.class, self))
        return;

    self._protectedContentListStore->~ContentRuleListStore();

    [super dealloc];
}

+ (instancetype)defaultStore
{
#if ENABLE(CONTENT_EXTENSIONS)
    return wrapper(API::ContentRuleListStore::defaultStoreSingleton());
#else
    return nil;
#endif
}

+ (instancetype)storeWithURL:(NSURL *)url
{
#if ENABLE(CONTENT_EXTENSIONS)
    return wrapper(API::ContentRuleListStore::storeWithPath(url.absoluteURL.path)).autorelease();
#else
    return nil;
#endif
}

- (Ref<API::ContentRuleListStore>)_protectedContentListStore
{
    return *_contentRuleListStore;
}

- (void)compileContentRuleListForIdentifier:(NSString *)identifier encodedContentRuleList:(NSString *)encodedContentRuleList completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->compileContentRuleList(identifier, encodedContentRuleList, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        if (error) {
            RetainPtr userInfo = @{ NSHelpAnchorErrorKey: adoptNS([[NSString alloc] initWithFormat:@"Rule list compilation failed: %s", error.message().c_str()]).get() };

            // error.value() could have a specific compiler error that is not equal to WKErrorContentRuleListStoreCompileFailed.
            // We want to use error.message, but here we want to only pass on CompileFailed with userInfo from the std::error_code.
            return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorContentRuleListStoreCompileFailed userInfo:userInfo.get()]);
        }
        completionHandler(wrapper(WTFMove(contentRuleList)).get(), nil);
    });
#endif
}

- (void)lookUpContentRuleListForIdentifier:(NSString *)identifier completionHandler:(void (^)(WKContentRuleList *, NSError *))completionHandler
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->lookupContentRuleList(identifier, [completionHandler = makeBlockPtr(completionHandler)](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        if (error) {
            RetainPtr userInfo = @{ NSHelpAnchorErrorKey: adoptNS([[NSString alloc] initWithFormat:@"Rule list lookup failed: %s", error.message().c_str()]).get() };
            auto wkError = toWKErrorCode(error);
            ASSERT(wkError == WKErrorContentRuleListStoreLookUpFailed || wkError == WKErrorContentRuleListStoreVersionMismatch);
            return completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:wkError userInfo:userInfo.get()]);
        }

        completionHandler(wrapper(WTFMove(contentRuleList)).get(), nil);
    });
#endif
}

- (void)getAvailableContentRuleListIdentifiers:(void (^)(NSArray<NSString *>*))completionHandler
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->getAvailableContentRuleListIdentifiers([completionHandler = makeBlockPtr(completionHandler)](Vector<String> identifiers) {
        completionHandler(createNSArray(identifiers).get());
    });
#endif
}

- (void)removeContentRuleListForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->removeContentRuleList(identifier, [completionHandler = makeBlockPtr(completionHandler)](std::error_code error) {
        if (error) {
            RetainPtr userInfo = @{ NSHelpAnchorErrorKey: adoptNS([[NSString alloc] initWithFormat:@"Rule list removal failed: %s", error.message().c_str()]).get() };
            ASSERT(toWKErrorCode(error) == WKErrorContentRuleListStoreRemoveFailed);
            return completionHandler([NSError errorWithDomain:WKErrorDomain code:WKErrorContentRuleListStoreRemoveFailed userInfo:userInfo.get()]);
        }

        completionHandler(nil);
    });
#endif
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_contentRuleListStore;
}

@end

@implementation WKContentRuleListStore (WKPrivate)

// For testing only.

- (void)_removeAllContentRuleLists
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->synchronousRemoveAllContentRuleLists();
#endif
}

- (void)_invalidateContentRuleListVersionForIdentifier:(NSString *)identifier
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->invalidateContentRuleListVersion(identifier);
#endif
}

- (void)_corruptContentRuleListHeaderForIdentifier:(NSString *)identifier usingCurrentVersion:(BOOL)usingCurrentVersion
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->corruptContentRuleListHeader(identifier, usingCurrentVersion);
#endif
}

- (void)_corruptContentRuleListActionsMatchingEverythingForIdentifier:(NSString *)identifier
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->corruptContentRuleListActionsMatchingEverything(identifier);
#endif
}

- (void)_invalidateContentRuleListHeaderForIdentifier:(NSString *)identifier
{
#if ENABLE(CONTENT_EXTENSIONS)
    self._protectedContentListStore->invalidateContentRuleListHeader(identifier);
#endif
}

- (void)_getContentRuleListSourceForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSString*))completionHandler
{
#if ENABLE(CONTENT_EXTENSIONS)
    auto handler = adoptNS([completionHandler copy]);
    self._protectedContentListStore->getContentRuleListSource(identifier, [handler](String source) {
        auto rawHandler = (void (^)(NSString *))handler.get();
        if (source.isNull()) {
            // This should not be necessary since there are no nullability annotations
            // in this file or any other unified source combined herein.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
            rawHandler(nil);
#pragma clang diagnostic pop
        } else
            rawHandler(source.createNSString().get());
    });
#endif
}

+ (void)_setContentRuleListStoreForResourceMonitorURLsControllerForTesting:(WKContentRuleListStore *)store
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    RetainPtr protectedStore = store;
    Ref apiStore = *(protectedStore->_contentRuleListStore);
    WebKit::ResourceMonitorURLsController::singleton().setContentRuleListStore(apiStore.get());
#else
    UNUSED_PARAM(store);
#endif
}

+ (instancetype)defaultStoreWithLegacyFilename
{
#if ENABLE(CONTENT_EXTENSIONS)
    return wrapper(API::ContentRuleListStore::defaultStoreSingleton());
#else
    return nil;
#endif
}

+ (instancetype)storeWithURLAndLegacyFilename:(NSURL *)url
{
#if ENABLE(CONTENT_EXTENSIONS)
    return wrapper(API::ContentRuleListStore::storeWithPath(url.absoluteURL.path)).autorelease();
#else
    return nil;
#endif
}

- (void)removeContentExtensionForIdentifier:(NSString *)identifier completionHandler:(void (^)(NSError *))completionHandler
{
    [self removeContentRuleListForIdentifier:identifier completionHandler:completionHandler];
}

@end
