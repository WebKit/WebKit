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

// Page cache.
static const char FeatureCounterPageCacheFailureNoDocumentLoaderKey[] = "com.apple.WebKit.pageCache.failure.noDocumentLoader";
static const char FeatureCounterPageCacheFailureMainDocumentErrorKey[] = "com.apple.WebKit.pageCache.failure.mainDocumentError";
static const char FeatureCounterPageCacheFailureIsErrorPageKey[] = "com.apple.WebKit.pageCache.failure.isErrorPage";
static const char FeatureCounterPageCacheFailureHasPlugins[] = "com.apple.WebKit.pageCache.failure.hasPlugins";
static const char FeatureCounterPageCacheFailureHTTPSNoCacheKey[] = "com.apple.WebKit.pageCache.failure.httpsNoCache";
static const char FeatureCounterPageCacheFailureHTTPSNoStoreKey[] = "com.apple.WebKit.pageCache.failure.httpsNoStore";
#if ENABLE(SQL_DATABASE)
static const char FeatureCounterPageCacheFailureHasOpenDatabasesKey[] = "com.apple.WebKit.pageCache.failure.hasOpenDatabases";
#endif
#if ENABLE(SHARED_WORKERS)
static const char FeatureCounterPageCacheFailureHasSharedWorkersKey[] = "com.apple.WebKit.pageCache.failure.hasSharedWorkers";
#endif
static const char FeatureCounterPageCacheFailureNoCurrentHistoryItemKey[] = "com.apple.WebKit.pageCache.failure.noCurrentHistoryItem";
static const char FeatureCounterPageCacheFailureQuirkRedirectComingKey[] = "com.apple.WebKit.pageCache.failure.quirkRedirectComing";
static const char FeatureCounterPageCacheFailureLoadingAPISenseKey[] = "com.apple.WebKit.pageCache.failure.loadingAPISense";
static const char FeatureCounterPageCacheFailureDocumentLoaderStoppingKey[] = "com.apple.WebKit.pageCache.failure.documentLoaderStopping";
static const char FeatureCounterPageCacheFailureCannotSuspendActiveDOMObjectsKey[] = "com.apple.WebKit.pageCache.failure.cannotSuspendActiveDOMObjects";
static const char FeatureCounterPageCacheFailureApplicationCacheKey[] = "com.apple.WebKit.pageCache.failure.applicationCache";
static const char FeatureCounterPageCacheFailureDeniedByClientKey[] = "com.apple.WebKit.pageCache.failure.deniedByClient";
#if ENABLE(DEVICE_ORIENTATION)
static const char FeatureCounterPageCacheFailureDeviceMotionKey[] = "com.apple.WebKit.pageCache.failure.deviceMotion";
static const char FeatureCounterPageCacheFailureDeviceOrientationKey[] = "com.apple.WebKit.pageCache.failure.deviceOrientation";
#endif
#if ENABLE(PROXIMITY_EVENTS)
static const char FeatureCounterPageCacheFailureDeviceProximityKey[] = "com.apple.WebKit.pageCache.failure.deviceProximity";
#endif
static const char FeatureCounterPageCacheFailureReloadKey[] = "com.apple.WebKit.pageCache.failure.reload";
static const char FeatureCounterPageCacheFailureReloadFromOriginKey[] = "com.apple.WebKit.pageCache.failure.reloadFromOrigin";
static const char FeatureCounterPageCacheFailureSameLoadKey[] = "com.apple.WebKit.pageCache.failure.sameLoad";
static const char FeatureCounterPageCacheFailureExpiredKey[] = "com.apple.WebKit.pageCache.failure.expired";
static const char FeatureCounterPageCacheFailurePrunedMemoryPressureKey[] = "com.apple.WebKit.pageCache.failure.pruned.memoryPressure";
static const char FeatureCounterPageCacheFailurePrunedCapacityReachedKey[] = "com.apple.WebKit.pageCache.failure.pruned.capacityReached";
static const char FeatureCounterPageCacheFailurePrunedProcessedSuspendedKey[] = "com.apple.WebKit.pageCache.failure.pruned.processSuspended";
static const char FeatureCounterPageCacheFailureKey[] = "com.apple.WebKit.pageCache.failure";
static const char FeatureCounterPageCacheSuccessKey[] = "com.apple.WebKit.pageCache.success";

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

// Navigation types.
static const char FeatureCounterNavigationStandard[] = "com.apple.WebKit.navigation.standard";
static const char FeatureCounterNavigationBack[] = "com.apple.WebKit.navigation.back";
static const char FeatureCounterNavigationForward[] = "com.apple.WebKit.navigation.forward";
static const char FeatureCounterNavigationIndexedBackForward[] = "com.apple.WebKit.navigation.indexedBackForward";
static const char FeatureCounterNavigationReload[] = "com.apple.WebKit.navigation.reload";
static const char FeatureCounterNavigationSame[] = "com.apple.WebKit.navigation.same";
static const char FeatureCounterNavigationReloadFromOrigin[] = "com.apple.WebKit.navigation.reloadFromOrigin";

// Resource types.
static const char FeatureCounterResourceLoadedFontKey[] = "com.apple.WebKit.resource.loaded.font";
static const char FeatureCounterResourceLoadedImageKey[] = "com.apple.WebKit.resource.loaded.image";
static const char FeatureCounterResourceLoadedMainResourceKey[] = "com.apple.WebKit.resource.loaded.mainResource";
static const char FeatureCounterResourceLoadedRawKey[] = "com.apple.WebKit.resource.loaded.raw";
static const char FeatureCounterResourceLoadedSVGDocumentKey[] = "com.apple.WebKit.resource.loaded.svgDocument";
static const char FeatureCounterResourceLoadedScriptKey[] = "com.apple.WebKit.resource.loaded.script";
static const char FeatureCounterResourceLoadedStyleSheetKey[] = "com.apple.WebKit.resource.loaded.styleSheet";
static const char FeatureCounterResourceLoadedOtherKey[] = "com.apple.WebKit.resource.loaded.other";

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

