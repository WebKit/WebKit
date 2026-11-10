/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "DiagnosticLoggingKeys.h"

namespace WebCore {

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaLoadedKey()
{
    return "mediaLoaded"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaLoadingFailedKey()
{
    return "mediaFailedLoading"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::memoryCacheEntryDecisionKey()
{
    return "memoryCacheEntryDecision"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::memoryCacheUsageKey()
{
    return "memoryCacheUsage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::missingValidatorFieldsKey()
{
    return "missingValidatorFields"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::postPageBackgroundingCPUUsageKey()
{
    return "postPageBackgroundingCPUUsage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::postPageBackgroundingMemoryUsageKey()
{
    return "postPageBackgroundingMemoryUsage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::postPageLoadCPUUsageKey()
{
    return "postPageLoadCPUUsage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::postPageLoadMemoryUsageKey()
{
    return "postPageLoadMemoryUsage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::provisionalLoadKey()
{
    return "provisionalLoad"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::pageContainsMediaEngineKey()
{
    return "pageContainsMediaEngine"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::pageContainsAtLeastOneMediaEngineKey()
{
    return "pageContainsAtLeastOneMediaEngine"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::pageLoadedKey()
{
    return "pageLoaded"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::playedKey()
{
    return "played"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::engineFailedToLoadKey()
{
    return "engineFailedToLoad"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::entryRightlyNotWarmedUpKey()
{
    return "entryRightlyNotWarmedUp"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::entryWronglyNotWarmedUpKey()
{
    return "entryWronglyNotWarmedUp"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::navigationKey()
{
    return "navigation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::needsRevalidationKey()
{
    return "needsRevalidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::networkCacheKey()
{
    return "networkCache"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::networkCacheFailureReasonKey()
{
    return "networkCacheFailureReason"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::networkCacheUnusedReasonKey()
{
    return "networkCacheUnusedReason"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::networkCacheReuseFailureKey()
{
    return "networkCacheReuseFailure"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::networkKey()
{
    return "network"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::networkProcessCrashedKey()
{
    return "networkProcessCrashed"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::neverSeenBeforeKey()
{
    return "neverSeenBefore"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::noKey()
{
    return "no"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::nonVisibleStateKey()
{
    return "nonVisibleState"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::backForwardCacheKey()
{
    return "backForwardCache"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::backForwardCacheFailureKey()
{
    return "backForwardCacheFailure"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::visuallyEmptyKey()
{
    return "visuallyEmpty"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::noDocumentLoaderKey()
{
    return "noDocumentLoader"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::noLongerInCacheKey()
{
    return "noLongerInCache"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::otherKey()
{
    return "other"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mainResourceKey()
{
    return "mainResource"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::isErrorPageKey()
{
    return "isErrorPage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::isReloadIgnoringCacheDataKey()
{
    return "isReloadIgnoringCacheData"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::loadingKey()
{
    return "loading"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::httpsNoStoreKey()
{
    return "httpsNoStore"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::imageKey()
{
    return "image"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::internalErrorKey()
{
    return "internalError"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::invalidSessionIDKey()
{
    return "invalidSessionID"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::isAttachmentKey()
{
    return "isAttachment"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::isConditionalRequestKey()
{
    return "isConditionalRequest"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::isDisabledKey()
{
    return "isDisabled"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::noCurrentHistoryItemKey()
{
    return "noCurrentHistoryItem"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::quirkRedirectComingKey()
{
    return "quirkRedirectComing"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::rawKey()
{
    return "raw"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::redirectKey()
{
    return "redirect"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::isLoadingKey()
{
    return "isLoading"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::documentLoaderStoppingKey()
{
    return "documentLoaderStopping"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::domainCausingCrashKey()
{
    return "DomainCausingCrash"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::domainCausingEnergyDrainKey()
{
    return "DomainCausingEnergyDrain"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::domainCausingJetsamKey()
{
    return "DomainCausingJetsam"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::simulatedPageCrashKey()
{
    return "SimulatedPageCrash"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::exceededActiveMemoryLimitKey()
{
    return "ExceededActiveMemoryLimit"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::exceededInactiveMemoryLimitKey()
{
    return "ExceededInactiveMemoryLimit"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::exceededBackgroundCPULimitKey()
{
    return "ExceededBackgroundCPULimit"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::domainVisitedKey()
{
    return "DomainVisited"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey()
{
    return "cannotSuspendActiveDOMObjects"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::cpuUsageKey()
{
    return "cpuUsage"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::createSharedBufferFailedKey()
{
    return "createSharedBufferFailed"_si;
}

#if ENABLE(APPLICATION_MANIFEST)
RefPtr<StringImpl> DiagnosticLoggingKeys::applicationManifestKey()
{
    return "applicationManifest"_si;
}
#endif

RefPtr<StringImpl> DiagnosticLoggingKeys::audioKey()
{
    return "audio"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::backNavigationDeltaKey()
{
    return "backNavigationDelta"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::canCacheKey()
{
    return "canCache"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::cacheControlNoStoreKey()
{
    return "cacheControlNoStore"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::cachedResourceRevalidationKey()
{
    return "cachedResourceRevalidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::cachedResourceRevalidationReasonKey()
{
    return "cachedResourceRevalidationReason"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::deniedByClientKey()
{
    return "deniedByClient"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::deviceMotionKey()
{
    return "deviceMotion"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::deviceOrientationKey()
{
    return "deviceOrientation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::diskCacheKey()
{
    return "diskCache"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::diskCacheAfterValidationKey()
{
    return "diskCacheAfterValidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::memoryCacheKey()
{
    return "memoryCache"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::memoryCacheAfterValidationKey()
{
    return "memoryCacheAfterValidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::reloadKey()
{
    return "reload"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::replaceKey()
{
    return "replace"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::retrievalRequestKey()
{
    return "retrievalRequest"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::resourceLoadedKey()
{
    return "resourceLoaded"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::resourceResponseSourceKey()
{
    return "resourceResponseSource"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::retrievalKey()
{
    return "retrieval"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::reloadFromOriginKey()
{
    return "reloadFromOrigin"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::reloadRevalidatingExpiredKey()
{
    return "reloadRevalidatingExpired"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::sameLoadKey()
{
    return "sameLoad"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::scriptKey()
{
    return "script"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::serviceWorkerKey()
{
    return "serviceWorker"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::siteSpecificQuirkKey()
{
    return "siteSpecificQuirk"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::streamingMedia()
{
    return "streamingMedia"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::styleSheetKey()
{
    return "styleSheet"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey()
{
    return "successfulSpeculativeWarmupWithRevalidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey()
{
    return "successfulSpeculativeWarmupWithoutRevalidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::svgDocumentKey()
{
    return "svgDocument"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::synchronousMessageFailedKey()
{
    return "synchronousMessageFailed"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::telemetryPageLoadKey()
{
    return "telemetryPageLoad"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::timedOutKey()
{
    return "timedOut"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::canceledLessThan2SecondsKey()
{
    return "canceledLessThan2Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::canceledLessThan5SecondsKey()
{
    return "canceledLessThan5Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::canceledLessThan20SecondsKey()
{
    return "canceledLessThan20Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::canceledMoreThan20SecondsKey()
{
    return "canceledMoreThan20Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::failedLessThan2SecondsKey()
{
    return "failedLessThan2Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::failedLessThan5SecondsKey()
{
    return "failedLessThan5Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::failedLessThan20SecondsKey()
{
    return "failedLessThan20Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::failedMoreThan20SecondsKey()
{
    return "failedMoreThan20Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::occurredKey()
{
    return "occurred"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::succeededLessThan2SecondsKey()
{
    return "succeededLessThan2Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::succeededLessThan5SecondsKey()
{
    return "succeededLessThan5Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::succeededLessThan20SecondsKey()
{
    return "succeededLessThan20Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::succeededMoreThan20SecondsKey()
{
    return "succeededMoreThan20Seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::uncacheableStatusCodeKey()
{
    return "uncacheableStatusCode"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::underMemoryPressureKey()
{
    return "underMemoryPressure"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unknownEntryRequestKey()
{
    return "unknownEntryRequest"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unlikelyToReuseKey()
{
    return "unlikelyToReuse"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unsupportedHTTPMethodKey()
{
    return "unsupportedHTTPMethod"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unsuspendableDOMObjectKey()
{
    return "unsuspendableDOMObject"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unusedKey()
{
    return "unused"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::usedKey()
{
    return "used"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::userZoomActionKey()
{
    return "userZoomAction"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::varyingHeaderMismatchKey()
{
    return "varyingHeaderMismatch"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::videoKey()
{
    return "video"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::visibleNonActiveStateKey()
{
    return "visibleNonActiveState"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::visibleAndActiveStateKey()
{
    return "visibleAndActiveState"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey()
{
    return "wastedSpeculativeWarmupWithRevalidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::wastedSpeculativeWarmupWithoutRevalidationKey()
{
    return "wastedSpeculativeWarmupWithoutRevalidation"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::webViewKey()
{
    return "webView"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::yesKey()
{
    return "yes"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::expiredKey()
{
    return "expired"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::fontKey()
{
    return "font"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::prunedDueToMemoryPressureKey()
{
    return "pruned.memoryPressure"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::prunedDueToMaxSizeReached()
{
    return "pruned.capacityReached"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::prunedDueToProcessSuspended()
{
    return "pruned.processSuspended"_si;
}

RefPtr<StringImpl> WebCore::DiagnosticLoggingKeys::notHTTPFamilyKey()
{
    return "notHTTPFamily"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::memoryUsageToDiagnosticLoggingKey(uint64_t memoryUsage)
{
    if (memoryUsage < 32 * MB)
        return "below32"_si;
    if (memoryUsage < 64 * MB)
        return "32to64"_si;
    if (memoryUsage < 128 * MB)
        return "64to128"_si;
    if (memoryUsage < 256 * MB)
        return "128to256"_si;
    if (memoryUsage < 512 * MB)
        return "256to512"_si;
    if (memoryUsage < 1024 * MB)
        return "512to1024"_si;
    if (memoryUsage < 2048 * MB)
        return "1024to2048"_si;
    if (memoryUsage < 4096llu * MB)
        return "2048to4096"_si;
    if (memoryUsage < 8192llu * MB)
        return "4096to8192"_si;
    if (memoryUsage < 16384llu * MB)
        return "8192to16384"_si;
    if (memoryUsage < 32768llu * MB)
        return "16384to32768"_si;
    return "over32768"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::foregroundCPUUsageToDiagnosticLoggingKey(double cpuUsage)
{
    if (cpuUsage < 10)
        return "below10"_si;
    if (cpuUsage < 20)
        return "10to20"_si;
    if (cpuUsage < 40)
        return "20to40"_si;
    if (cpuUsage < 60)
        return "40to60"_si;
    if (cpuUsage < 80)
        return "60to80"_si;
    return "over80"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::backgroundCPUUsageToDiagnosticLoggingKey(double cpuUsage)
{
    if (cpuUsage < 1)
        return "below1"_si;
    if (cpuUsage < 5)
        return "1to5"_si;
    if (cpuUsage < 10)
        return "5to10"_si;
    if (cpuUsage < 30)
        return "10to30"_si;
    if (cpuUsage < 50)
        return "30to50"_si;
    if (cpuUsage < 70)
        return "50to70"_si;
    return "over70"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaSourceTypeWatchTimeKey()
{
    return "watchtimeBySource"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaVideoCodecWatchTimeKey()
{
    return "watchtimeByVideoCodec"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaAudioCodecWatchTimeKey()
{
    return "watchtimeByAudioCodec"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaBufferingWatchTimeKey()
{
    return "watchtimeBuffering"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaTextTrackWatchTimeKey()
{
    return "watchtimeTextTrack"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::secondsKey()
{
    return "seconds"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::sourceTypeKey()
{
    return "sourceType"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::videoCodecKey()
{
    return "videoCodec"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::audioCodecKey()
{
    return "audioCodec"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::textTrackTypeKey()
{
    return "textTrackType"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::textTrackKindKey()
{
    return "textTrackKind"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::textTrackModeKey()
{
    return "textTrackMode"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::mediaPresentationTypeWatchTimeKey()
{
    return "watchtimeByPresentationType"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::presentationTypeKey()
{
    return "presentationType"_si;
}

#if ENABLE(CONTENT_EXTENSIONS)
RefPtr<StringImpl> DiagnosticLoggingKeys::iframeResourceMonitoringKey()
{
    return "IFrameResourceMonitoring"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unloadCountKey()
{
    return "unloadCount"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unloadPreventedByThrottlerCountKey()
{
    return "unloadPreventedByThrottlerCount"_si;
}

RefPtr<StringImpl> DiagnosticLoggingKeys::unloadPreventedByStickyActivationCountKey()
{
    return "unloadPreventedByStickyActivationCount"_si;
}
#endif

} // namespace WebCore

