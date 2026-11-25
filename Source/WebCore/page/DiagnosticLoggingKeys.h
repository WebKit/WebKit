/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/text/WTFString.h>

namespace WebCore {

enum class HTMLMediaElementSourceType : uint8_t;

class DiagnosticLoggingKeys {
public:
#if ENABLE(APPLICATION_MANIFEST)
    static RefPtr<StringImpl> applicationManifestKey();
#endif
    static RefPtr<StringImpl> audioKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> backNavigationDeltaKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> cacheControlNoStoreKey();
    static RefPtr<StringImpl> cachedResourceRevalidationKey();
    static RefPtr<StringImpl> cachedResourceRevalidationReasonKey();
    static RefPtr<StringImpl> canCacheKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> canceledLessThan2SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> canceledLessThan5SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> canceledLessThan20SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> canceledMoreThan20SecondsKey();
    static RefPtr<StringImpl> cannotSuspendActiveDOMObjectsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> cpuUsageKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> createSharedBufferFailedKey();
    static RefPtr<StringImpl> deniedByClientKey();
    static RefPtr<StringImpl> deviceMotionKey();
    static RefPtr<StringImpl> deviceOrientationKey();
    static RefPtr<StringImpl> diskCacheKey();
    static RefPtr<StringImpl> diskCacheAfterValidationKey();
    static RefPtr<StringImpl> memoryCacheKey();
    static RefPtr<StringImpl> memoryCacheAfterValidationKey();
    static RefPtr<StringImpl> documentLoaderStoppingKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> domainCausingCrashKey();
    static RefPtr<StringImpl> domainCausingEnergyDrainKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> domainCausingJetsamKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> simulatedPageCrashKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> exceededActiveMemoryLimitKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> exceededInactiveMemoryLimitKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> exceededBackgroundCPULimitKey();
    static RefPtr<StringImpl> domainVisitedKey();
    static RefPtr<StringImpl> engineFailedToLoadKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> entryRightlyNotWarmedUpKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> entryWronglyNotWarmedUpKey();
    static RefPtr<StringImpl> expiredKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> failedLessThan2SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> failedLessThan5SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> failedLessThan20SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> failedMoreThan20SecondsKey();
    static RefPtr<StringImpl> fontKey();
    static RefPtr<StringImpl> httpsNoStoreKey();
    static RefPtr<StringImpl> imageKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> internalErrorKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> invalidSessionIDKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> isAttachmentKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> isConditionalRequestKey();
    static RefPtr<StringImpl> isDisabledKey();
    static RefPtr<StringImpl> isErrorPageKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> isReloadIgnoringCacheDataKey();
    static RefPtr<StringImpl> loadingKey();
    static RefPtr<StringImpl> isLoadingKey();
    static RefPtr<StringImpl> mainResourceKey();
    static RefPtr<StringImpl> mediaLoadedKey();
    static RefPtr<StringImpl> mediaLoadingFailedKey();
    static RefPtr<StringImpl> memoryCacheEntryDecisionKey();
    static RefPtr<StringImpl> memoryCacheUsageKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> missingValidatorFieldsKey();
    static RefPtr<StringImpl> navigationKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> needsRevalidationKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> networkCacheKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> networkCacheFailureReasonKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> networkCacheUnusedReasonKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> networkCacheReuseFailureKey();
    static RefPtr<StringImpl> networkKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> networkProcessCrashedKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> neverSeenBeforeKey();
    static RefPtr<StringImpl> noKey();
    static RefPtr<StringImpl> noCurrentHistoryItemKey();
    static RefPtr<StringImpl> noDocumentLoaderKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> noLongerInCacheKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> nonVisibleStateKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> notHTTPFamilyKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> occurredKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> otherKey();
    static RefPtr<StringImpl> backForwardCacheKey();
    static RefPtr<StringImpl> backForwardCacheFailureKey();
    static RefPtr<StringImpl> visuallyEmptyKey();
    static RefPtr<StringImpl> pageContainsAtLeastOneMediaEngineKey();
    static RefPtr<StringImpl> pageContainsMediaEngineKey();
    static RefPtr<StringImpl> pageLoadedKey();
    static RefPtr<StringImpl> playedKey();
    static RefPtr<StringImpl> postPageBackgroundingCPUUsageKey();
    static RefPtr<StringImpl> postPageBackgroundingMemoryUsageKey();
    static RefPtr<StringImpl> postPageLoadCPUUsageKey();
    static RefPtr<StringImpl> postPageLoadMemoryUsageKey();
    static RefPtr<StringImpl> provisionalLoadKey();
    static RefPtr<StringImpl> prunedDueToMaxSizeReached();
    static RefPtr<StringImpl> prunedDueToMemoryPressureKey();
    static RefPtr<StringImpl> prunedDueToProcessSuspended();
    static RefPtr<StringImpl> quirkRedirectComingKey();
    static RefPtr<StringImpl> rawKey();
    static RefPtr<StringImpl> redirectKey();
    static RefPtr<StringImpl> reloadFromOriginKey();
    static RefPtr<StringImpl> reloadKey();
    static RefPtr<StringImpl> reloadRevalidatingExpiredKey();
    static RefPtr<StringImpl> replaceKey();
    static RefPtr<StringImpl> resourceLoadedKey();
    static RefPtr<StringImpl> resourceResponseSourceKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> retrievalKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> retrievalRequestKey();
    static RefPtr<StringImpl> sameLoadKey();
    static RefPtr<StringImpl> scriptKey();
    static RefPtr<StringImpl> serviceWorkerKey();
    static RefPtr<StringImpl> siteSpecificQuirkKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> streamingMedia();
    static RefPtr<StringImpl> styleSheetKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> succeededLessThan2SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> succeededLessThan5SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> succeededLessThan20SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> succeededMoreThan20SecondsKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> successfulSpeculativeWarmupWithRevalidationKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> successfulSpeculativeWarmupWithoutRevalidationKey();
    static RefPtr<StringImpl> svgDocumentKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> synchronousMessageFailedKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> telemetryPageLoadKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> timedOutKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> uncacheableStatusCodeKey();
    static RefPtr<StringImpl> underMemoryPressureKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> unknownEntryRequestKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> unlikelyToReuseKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> unsupportedHTTPMethodKey();
    static RefPtr<StringImpl> unsuspendableDOMObjectKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> unusedKey();
    static RefPtr<StringImpl> usedKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> userZoomActionKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> varyingHeaderMismatchKey();
    static RefPtr<StringImpl> videoKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> visibleNonActiveStateKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> visibleAndActiveStateKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> wastedSpeculativeWarmupWithRevalidationKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> wastedSpeculativeWarmupWithoutRevalidationKey();
    WEBCORE_EXPORT static RefPtr<StringImpl> webViewKey();
    static RefPtr<StringImpl> yesKey();

    static RefPtr<StringImpl> mediaSourceTypeWatchTimeKey();
    static RefPtr<StringImpl> mediaVideoCodecWatchTimeKey();
    static RefPtr<StringImpl> mediaAudioCodecWatchTimeKey();
    static RefPtr<StringImpl> mediaBufferingWatchTimeKey();
    static RefPtr<StringImpl> mediaTextTrackWatchTimeKey();

    static RefPtr<StringImpl> secondsKey();
    static RefPtr<StringImpl> sourceTypeKey();
    static RefPtr<StringImpl> videoCodecKey();
    static RefPtr<StringImpl> audioCodecKey();
    static RefPtr<StringImpl> textTrackTypeKey();
    static RefPtr<StringImpl> textTrackKindKey();
    static RefPtr<StringImpl> textTrackModeKey();

    static RefPtr<StringImpl> mediaPresentationTypeWatchTimeKey();
    static RefPtr<StringImpl> presentationTypeKey();

    WEBCORE_EXPORT static RefPtr<StringImpl> memoryUsageToDiagnosticLoggingKey(uint64_t memoryUsage);
    WEBCORE_EXPORT static RefPtr<StringImpl> foregroundCPUUsageToDiagnosticLoggingKey(double cpuUsage);
    WEBCORE_EXPORT static RefPtr<StringImpl> backgroundCPUUsageToDiagnosticLoggingKey(double cpuUsage);

#if ENABLE(CONTENT_EXTENSIONS)
    static RefPtr<StringImpl> iframeResourceMonitoringKey();
    static RefPtr<StringImpl> unloadCountKey();
    static RefPtr<StringImpl> unloadPreventedByThrottlerCountKey();
    static RefPtr<StringImpl> unloadPreventedByStickyActivationCountKey();
#endif
};

} // namespace WebCore
