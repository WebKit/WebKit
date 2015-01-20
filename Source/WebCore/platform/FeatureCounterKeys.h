/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FeatureCounterKeys_h
#define FeatureCounterKeys_h

namespace WebCore {

// Cached resources revalidation.
static const char FeatureCounterCachedResourceRevalidationSuccessKey[] = "com.apple.WebKit.cachedResourceRevalidation.success";
static const char FeatureCounterCachedResourceRevalidationFailureKey[] = "com.apple.WebKit.cachedResourceRevalidation.failure";
static const char FeatureCounterCachedResourceRevalidationReasonReloadKey[] = "com.apple.WebKit.cachedResourceRevalidation.reason.reload";
static const char FeatureCounterCachedResourceRevalidationReasonNoCacheKey[] = "com.apple.WebKit.cachedResourceRevalidation.reason.noCache";
static const char FeatureCounterCachedResourceRevalidationReasonNoStoreKey[] = "com.apple.WebKit.cachedResourceRevalidation.reason.noStore";
static const char FeatureCounterCachedResourceRevalidationReasonMustRevalidateIsExpiredKey[] = "com.apple.WebKit.cachedResourceRevalidation.reason.mustRevalidateIsExpired";
static const char FeatureCounterCachedResourceRevalidationReasonIsExpiredKey[] = "com.apple.WebKit.cachedResourceRevalidation.reason.isExpired";

// Media playback.
static const char FeatureCounterMediaVideoElementLoadingKey[] = "com.apple.WebKit.media.video.loading";
static const char FeatureCounterMediaAudioElementLoadingKey[] = "com.apple.WebKit.media.audio.loading";
static const char FeatureCounterMediaVideoElementPlayedKey[] = "com.apple.WebKit.media.video.played";
static const char FeatureCounterMediaAudioElementPlayedKey[] = "com.apple.WebKit.media.audio.played";

// Memory cache.
static const char FeatureCounterResourceRequestInMemoryCacheKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache";
static const char FeatureCounterResourceRequestNotInMemoryCacheKey[] = "com.apple.WebKit.resourceRequest.notInMemoryCache";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused";
static const char FeatureCounterResourceRequestInMemoryCacheUsedKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.used";
static const char FeatureCounterResourceRequestInMemoryCacheRevalidatingKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.revalidating";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonNoStoreKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.noStore";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonTypeMismatchKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.typeMismatch";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonRedirectChainKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.redirectChain";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonCredentialSettingsKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.credentialSettings";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonReloadKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.reload";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonErrorKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.error";
static const char FeatureCounterResourceRequestInMemoryCacheUnusedReasonMustRevalidateNoValidatorKey[] = "com.apple.WebKit.resourceRequest.inMemoryCache.unused.reason.mustRevalidateNoValidator";

// WebView user actions.
static const char FeatureCounterWebViewUserZoomedKey[] = "com.apple.WebKit.webView.user.zoomed";

} // namespace WebCore

#endif // FeatureCounterKeys_h

