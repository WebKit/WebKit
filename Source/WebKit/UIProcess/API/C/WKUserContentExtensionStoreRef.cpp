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

#include "config.h"
#include "WKUserContentExtensionStoreRef.h"

#include "APIContentRuleList.h"
#include "APIContentRuleListStore.h"
#include "WKAPICast.h"
#include <wtf/CompletionHandler.h>

using namespace WebKit;

WKTypeID WKUserContentExtensionStoreGetTypeID()
{
#if ENABLE(CONTENT_EXTENSIONS)
    return toAPI(API::ContentRuleListStore::APIType);
#else
    return 0;
#endif
}

WKUserContentExtensionStoreRef WKUserContentExtensionStoreCreate(WKStringRef path)
{
#if ENABLE(CONTENT_EXTENSIONS)
    return toAPI(&API::ContentRuleListStore::storeWithPath(toWTFString(path), false).leakRef());
#else
    UNUSED_PARAM(path);
    return nullptr;
#endif
}

#if ENABLE(CONTENT_EXTENSIONS)
static inline WKUserContentExtensionStoreResult toResult(const std::error_code& error)
{
    if (!error)
        return kWKUserContentExtensionStoreSuccess;

    switch (static_cast<API::ContentRuleListStore::Error>(error.value())) {
    case API::ContentRuleListStore::Error::LookupFailed:
        return kWKUserContentExtensionStoreLookupFailed;
    case API::ContentRuleListStore::Error::VersionMismatch:
        return kWKUserContentExtensionStoreVersionMismatch;
    case API::ContentRuleListStore::Error::CompileFailed:
        return kWKUserContentExtensionStoreCompileFailed;
    case API::ContentRuleListStore::Error::RemoveFailed:
        return kWKUserContentExtensionStoreRemoveFailed;
    }
}
#endif

void WKUserContentExtensionStoreCompile(WKUserContentExtensionStoreRef store, WKStringRef identifier, WKStringRef jsonSource, void* context, WKUserContentExtensionStoreFunction callback)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(store)->compileContentRuleList(toWTFString(identifier), toWTFString(jsonSource), [context, callback](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        callback(error ? nullptr : toAPI(contentRuleList.leakRef()), toResult(error), context);
    });
#else
    UNUSED_PARAM(jsonSource);
    callback(nullptr, kWKUserContentExtensionStoreCompileFailed, context);
#endif
}

void WKUserContentExtensionStoreLookup(WKUserContentExtensionStoreRef store, WKStringRef identifier, void* context, WKUserContentExtensionStoreFunction callback)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(store)->lookupContentRuleList(toWTFString(identifier), [context, callback](RefPtr<API::ContentRuleList> contentRuleList, std::error_code error) {
        callback(error ? nullptr : toAPI(contentRuleList.leakRef()), toResult(error), context);
    });
#else
    callback(nullptr, kWKUserContentExtensionStoreLookupFailed, context);
#endif
}

void WKUserContentExtensionStoreRemove(WKUserContentExtensionStoreRef store, WKStringRef identifier, void* context, WKUserContentExtensionStoreFunction callback)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(store)->removeContentRuleList(toWTFString(identifier), [context, callback](std::error_code error) {
        callback(nullptr, toResult(error), context);
    });
#else
    callback(nullptr, kWKUserContentExtensionStoreRemoveFailed, context);
#endif
}
