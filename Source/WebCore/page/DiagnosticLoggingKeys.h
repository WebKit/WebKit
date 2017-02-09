/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef DiagnosticLoggingKeys_h
#define DiagnosticLoggingKeys_h

#include <wtf/text/WTFString.h>

namespace WebCore {

class DiagnosticLoggingKeys {
public:
    static String applicationCacheKey();
    static String audioKey();
    WEBCORE_EXPORT static String backNavigationKey();
    WEBCORE_EXPORT static String cacheControlNoStoreKey();
    static String cachedResourceRevalidationKey();
    static String canCacheKey();
    static String cannotSuspendActiveDOMObjectsKey();
    WEBCORE_EXPORT static String deltaKey();
    static String deniedByClientKey();
    static String deviceMotionKey();
    static String deviceOrientationKey();
    static String deviceProximityKey();
    static String diskCacheKey();
    static String diskCacheAfterValidationKey();
    static String documentLoaderStoppingKey();
    static String engineFailedToLoadKey();
    WEBCORE_EXPORT static String entryRightlyNotWarmedUpKey();
    WEBCORE_EXPORT static String entryWronglyNotWarmedUpKey();
    static String expiredKey();
    static String fontKey();
    static String hasCalledWindowOpenKey();
    static String hasOpenerKey();
    static String hasPluginsKey();
    static String httpsNoStoreKey();
    static String imageKey();
    static String inMemoryCacheKey();
    WEBCORE_EXPORT static String isAttachmentKey();
    WEBCORE_EXPORT static String isConditionalRequestKey();
    static String isDisabledKey();
    static String isErrorPageKey();
    static String isExpiredKey();
    WEBCORE_EXPORT static String isReloadIgnoringCacheDataKey();
    static String loadedKey();
    static String loadingKey();
    static String isLoadingKey();
    static String mainDocumentErrorKey();
    static String mainResourceKey();
    static String mediaKey();
    static String mediaLoadedKey();
    static String mediaLoadingFailedKey();
    WEBCORE_EXPORT static String missingValidatorFieldsKey();
    static String navigationKey();
    WEBCORE_EXPORT static String needsRevalidationKey();
    WEBCORE_EXPORT static String networkCacheKey();
    static String networkKey();
    WEBCORE_EXPORT static String neverSeenBeforeKey();
    static String noCacheKey();
    static String noCurrentHistoryItemKey();
    static String noDocumentLoaderKey();
    WEBCORE_EXPORT static String noLongerInCacheKey();
    static String noStoreKey();
    WEBCORE_EXPORT static String notHTTPFamilyKey();
    WEBCORE_EXPORT static String notInCacheKey();
    static String notInMemoryCacheKey();
    WEBCORE_EXPORT static String otherKey();
    static String pageCacheKey();
    static String pageContainsAtLeastOneMediaEngineKey();
    static String pageContainsAtLeastOnePluginKey();
    static String pageContainsMediaEngineKey();
    static String pageContainsPluginKey();
    static String pageLoadedKey();
    static String playedKey();
    static String pluginLoadedKey();
    static String pluginLoadingFailedKey();
    static String provisionalLoadKey();
    static String prunedDueToMaxSizeReached();
    static String prunedDueToMemoryPressureKey();
    static String prunedDueToProcessSuspended();
    static String quirkRedirectComingKey();
    static String rawKey();
    static String reasonKey();
    static String redirectKey();
    static String reloadFromOriginKey();
    static String reloadKey();
    static String replaceKey();
    WEBCORE_EXPORT static String requestKey();
    static String resourceKey();
    static String resourceRequestKey();
    static String resourceResponseKey();
    WEBCORE_EXPORT static String retrievalKey();
    WEBCORE_EXPORT static String retrievalRequestKey();
    WEBCORE_EXPORT static String revalidatingKey();
    static String sameLoadKey();
    static String scriptKey();
    static String sourceKey();
    WEBCORE_EXPORT static String streamingMedia();
    static String styleSheetKey();
    WEBCORE_EXPORT static String successfulSpeculativeWarmupWithRevalidationKey();
    WEBCORE_EXPORT static String successfulSpeculativeWarmupWithoutRevalidationKey();
    static String svgDocumentKey();
    WEBCORE_EXPORT static String uncacheableStatusCodeKey();
    static String underMemoryPressureKey();
    WEBCORE_EXPORT static String unknownEntryRequestKey();
    WEBCORE_EXPORT static String unlikelyToReuseKey();
    WEBCORE_EXPORT static String unsupportedHTTPMethodKey();
    static String unsuspendableDOMObjectKey();
    WEBCORE_EXPORT static String unusableCachedEntryKey();
    WEBCORE_EXPORT static String unusedKey();
    static String unusedReasonCredentialSettingsKey();
    static String unusedReasonErrorKey();
    static String unusedReasonMustRevalidateNoValidatorKey();
    static String unusedReasonNoStoreKey();
    static String unusedReasonRedirectChainKey();
    static String unusedReasonReloadKey();
    static String unusedReasonTypeMismatchKey();
    static String usedKey();
    WEBCORE_EXPORT static String userKey();
    WEBCORE_EXPORT static String varyingHeaderMismatchKey();
    static String videoKey();
    WEBCORE_EXPORT static String wastedSpeculativeWarmupWithRevalidationKey();
    WEBCORE_EXPORT static String wastedSpeculativeWarmupWithoutRevalidationKey();
    WEBCORE_EXPORT static String webViewKey();
    WEBCORE_EXPORT static String zoomedKey();

    // Success / Failure keys.
    static String successKey();
    static String failureKey();
};

}

#endif // DiagnosticLoggingKeys_h
