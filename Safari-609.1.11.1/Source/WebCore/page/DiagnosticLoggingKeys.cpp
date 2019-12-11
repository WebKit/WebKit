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

#include "config.h"
#include "DiagnosticLoggingKeys.h"

namespace WebCore {

String DiagnosticLoggingKeys::mediaLoadedKey()
{
    return "mediaLoaded"_s;
}

String DiagnosticLoggingKeys::mediaLoadingFailedKey()
{
    return "mediaFailedLoading"_s;
}

String DiagnosticLoggingKeys::memoryCacheEntryDecisionKey()
{
    return "memoryCacheEntryDecision"_s;
}

String DiagnosticLoggingKeys::memoryCacheUsageKey()
{
    return "memoryCacheUsage"_s;
}

String DiagnosticLoggingKeys::missingValidatorFieldsKey()
{
    return "missingValidatorFields"_s;
}

String DiagnosticLoggingKeys::pluginLoadedKey()
{
    return "pluginLoaded"_s;
}

String DiagnosticLoggingKeys::pluginLoadingFailedKey()
{
    return "pluginFailedLoading"_s;
}

String DiagnosticLoggingKeys::postPageBackgroundingCPUUsageKey()
{
    return "postPageBackgroundingCPUUsage"_s;
}

String DiagnosticLoggingKeys::postPageBackgroundingMemoryUsageKey()
{
    return "postPageBackgroundingMemoryUsage"_s;
}

String DiagnosticLoggingKeys::pageHandlesWebGLContextLossKey()
{
    return "pageHandlesWebGLContextLoss"_s;
}

String DiagnosticLoggingKeys::postPageLoadCPUUsageKey()
{
    return "postPageLoadCPUUsage"_s;
}

String DiagnosticLoggingKeys::postPageLoadMemoryUsageKey()
{
    return "postPageLoadMemoryUsage"_s;
}

String DiagnosticLoggingKeys::provisionalLoadKey()
{
    return "provisionalLoad"_s;
}

String DiagnosticLoggingKeys::pageContainsPluginKey()
{
    return "pageContainsPlugin"_s;
}

String DiagnosticLoggingKeys::pageContainsAtLeastOnePluginKey()
{
    return "pageContainsAtLeastOnePlugin"_s;
}

String DiagnosticLoggingKeys::pageContainsMediaEngineKey()
{
    return "pageContainsMediaEngine"_s;
}

String DiagnosticLoggingKeys::pageContainsAtLeastOneMediaEngineKey()
{
    return "pageContainsAtLeastOneMediaEngine"_s;
}

String DiagnosticLoggingKeys::pageLoadedKey()
{
    return "pageLoaded"_s;
}

String DiagnosticLoggingKeys::playedKey()
{
    return "played"_s;
}

String DiagnosticLoggingKeys::engineFailedToLoadKey()
{
    return "engineFailedToLoad"_s;
}

String DiagnosticLoggingKeys::entryRightlyNotWarmedUpKey()
{
    return "entryRightlyNotWarmedUp"_s;
}

String DiagnosticLoggingKeys::entryWronglyNotWarmedUpKey()
{
    return "entryWronglyNotWarmedUp"_s;
}

String DiagnosticLoggingKeys::navigationKey()
{
    return "navigation"_s;
}

String DiagnosticLoggingKeys::needsRevalidationKey()
{
    return "needsRevalidation"_s;
}

String DiagnosticLoggingKeys::networkCacheKey()
{
    return "networkCache"_s;
}

String DiagnosticLoggingKeys::networkCacheFailureReasonKey()
{
    return "networkCacheFailureReason"_s;
}

String DiagnosticLoggingKeys::networkCacheUnusedReasonKey()
{
    return "networkCacheUnusedReason"_s;
}

String DiagnosticLoggingKeys::networkCacheReuseFailureKey()
{
    return "networkCacheReuseFailure"_s;
}

String DiagnosticLoggingKeys::networkKey()
{
    return "network"_s;
}

String DiagnosticLoggingKeys::networkProcessCrashedKey()
{
    return "networkProcessCrashed"_s;
}

String DiagnosticLoggingKeys::neverSeenBeforeKey()
{
    return "neverSeenBefore"_s;
}

String DiagnosticLoggingKeys::noKey()
{
    return "no"_s;
}

String DiagnosticLoggingKeys::noCacheKey()
{
    return "noCache"_s;
}

String DiagnosticLoggingKeys::noStoreKey()
{
    return "noStore"_s;
}

String DiagnosticLoggingKeys::nonVisibleStateKey()
{
    return "nonVisibleState"_s;
}

String DiagnosticLoggingKeys::notInMemoryCacheKey()
{
    return "notInMemoryCache"_s;
}

String DiagnosticLoggingKeys::backForwardCacheKey()
{
    return "backForwardCache"_s;
}

String DiagnosticLoggingKeys::backForwardCacheFailureKey()
{
    return "backForwardCacheFailure"_s;
}

String DiagnosticLoggingKeys::visuallyEmptyKey()
{
    return "visuallyEmpty"_s;
}

String DiagnosticLoggingKeys::noDocumentLoaderKey()
{
    return "noDocumentLoader"_s;
}

String DiagnosticLoggingKeys::noLongerInCacheKey()
{
    return "noLongerInCache"_s;
}

String DiagnosticLoggingKeys::otherKey()
{
    return "other"_s;
}

String DiagnosticLoggingKeys::mainResourceKey()
{
    return "mainResource"_s;
}

String DiagnosticLoggingKeys::isErrorPageKey()
{
    return "isErrorPage"_s;
}

String DiagnosticLoggingKeys::isExpiredKey()
{
    return "isExpired"_s;
}

String DiagnosticLoggingKeys::isReloadIgnoringCacheDataKey()
{
    return "isReloadIgnoringCacheData"_s;
}

String DiagnosticLoggingKeys::loadingKey()
{
    return "loading"_s;
}

String DiagnosticLoggingKeys::hasPluginsKey()
{
    return "hasPlugins"_s;
}

String DiagnosticLoggingKeys::httpsNoStoreKey()
{
    return "httpsNoStore"_s;
}

String DiagnosticLoggingKeys::imageKey()
{
    return "image"_s;
}

String DiagnosticLoggingKeys::inMemoryCacheKey()
{
    return "inMemoryCache"_s;
}

String DiagnosticLoggingKeys::inactiveKey()
{
    return "inactive"_s;
}

String DiagnosticLoggingKeys::internalErrorKey()
{
    return "internalError"_s;
}

String DiagnosticLoggingKeys::invalidSessionIDKey()
{
    return "invalidSessionID"_s;
}

String DiagnosticLoggingKeys::isAttachmentKey()
{
    return "isAttachment"_s;
}

String DiagnosticLoggingKeys::isConditionalRequestKey()
{
    return "isConditionalRequest"_s;
}

String DiagnosticLoggingKeys::isDisabledKey()
{
    return "isDisabled"_s;
}

String DiagnosticLoggingKeys::noCurrentHistoryItemKey()
{
    return "noCurrentHistoryItem"_s;
}

String DiagnosticLoggingKeys::quirkRedirectComingKey()
{
    return "quirkRedirectComing"_s;
}

String DiagnosticLoggingKeys::rawKey()
{
    return "raw"_s;
}

String DiagnosticLoggingKeys::redirectKey()
{
    return "redirect"_s;
}

String DiagnosticLoggingKeys::isLoadingKey()
{
    return "isLoading"_s;
}

String DiagnosticLoggingKeys::documentLoaderStoppingKey()
{
    return "documentLoaderStopping"_s;
}

String DiagnosticLoggingKeys::domainCausingCrashKey()
{
    return "DomainCausingCrash"_s;
}

String DiagnosticLoggingKeys::domainCausingEnergyDrainKey()
{
    return "DomainCausingEnergyDrain"_s;
}

String DiagnosticLoggingKeys::domainCausingJetsamKey()
{
    return "DomainCausingJetsam"_s;
}

String DiagnosticLoggingKeys::simulatedPageCrashKey()
{
    return "SimulatedPageCrash"_s;
}

String DiagnosticLoggingKeys::exceededActiveMemoryLimitKey()
{
    return "ExceededActiveMemoryLimit"_s;
}

String DiagnosticLoggingKeys::exceededInactiveMemoryLimitKey()
{
    return "ExceededInactiveMemoryLimit"_s;
}

String DiagnosticLoggingKeys::exceededBackgroundCPULimitKey()
{
    return "ExceededBackgroundCPULimit"_s;
}

String DiagnosticLoggingKeys::domainVisitedKey()
{
    return "DomainVisited"_s;
}

String DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey()
{
    return "cannotSuspendActiveDOMObjects"_s;
}

String DiagnosticLoggingKeys::cpuUsageKey()
{
    return "cpuUsage"_s;
}

String DiagnosticLoggingKeys::createSharedBufferFailedKey()
{
    return "createSharedBufferFailed"_s;
}

String DiagnosticLoggingKeys::activeInForegroundTabKey()
{
    return "activeInForegroundTab"_s;
}

String DiagnosticLoggingKeys::activeInBackgroundTabOnlyKey()
{
    return "activeInBackgroundTabOnly"_s;
}

String DiagnosticLoggingKeys::applicationCacheKey()
{
    return "applicationCache"_s;
}

#if ENABLE(APPLICATION_MANIFEST)
String DiagnosticLoggingKeys::applicationManifestKey()
{
    return "applicationManifest"_s;
}
#endif

String DiagnosticLoggingKeys::audioKey()
{
    return "audio"_s;
}

String DiagnosticLoggingKeys::backNavigationDeltaKey()
{
    return "backNavigationDelta"_s;
}

String DiagnosticLoggingKeys::canCacheKey()
{
    return "canCache"_s;
}

String DiagnosticLoggingKeys::cacheControlNoStoreKey()
{
    return "cacheControlNoStore"_s;
}

String DiagnosticLoggingKeys::cachedResourceRevalidationKey()
{
    return "cachedResourceRevalidation"_s;
}

String DiagnosticLoggingKeys::cachedResourceRevalidationReasonKey()
{
    return "cachedResourceRevalidationReason"_s;
}

String DiagnosticLoggingKeys::deniedByClientKey()
{
    return "deniedByClient"_s;
}

String DiagnosticLoggingKeys::deviceMotionKey()
{
    return "deviceMotion"_s;
}

String DiagnosticLoggingKeys::deviceOrientationKey()
{
    return "deviceOrientation"_s;
}

String DiagnosticLoggingKeys::diskCacheKey()
{
    return "diskCache"_s;
}

String DiagnosticLoggingKeys::diskCacheAfterValidationKey()
{
    return "diskCacheAfterValidation"_s;
}

String DiagnosticLoggingKeys::reloadKey()
{
    return "reload"_s;
}

String DiagnosticLoggingKeys::replaceKey()
{
    return "replace"_s;
}

String DiagnosticLoggingKeys::retrievalRequestKey()
{
    return "retrievalRequest"_s;
}

String DiagnosticLoggingKeys::resourceLoadedKey()
{
    return "resourceLoaded"_s;
}

String DiagnosticLoggingKeys::resourceResponseSourceKey()
{
    return "resourceResponseSource"_s;
}

String DiagnosticLoggingKeys::retrievalKey()
{
    return "retrieval"_s;
}

String DiagnosticLoggingKeys::revalidatingKey()
{
    return "revalidating"_s;
}

String DiagnosticLoggingKeys::reloadFromOriginKey()
{
    return "reloadFromOrigin"_s;
}

String DiagnosticLoggingKeys::reloadRevalidatingExpiredKey()
{
    return "reloadRevalidatingExpired"_s;
}

String DiagnosticLoggingKeys::sameLoadKey()
{
    return "sameLoad"_s;
}

String DiagnosticLoggingKeys::scriptKey()
{
    return "script"_s;
}

String DiagnosticLoggingKeys::serviceWorkerKey()
{
    return "serviceWorker"_s;
}

String DiagnosticLoggingKeys::siteSpecificQuirkKey()
{
    return "siteSpecificQuirk"_s;
}

String DiagnosticLoggingKeys::streamingMedia()
{
    return "streamingMedia"_s;
}

String DiagnosticLoggingKeys::styleSheetKey()
{
    return "styleSheet"_s;
}

String DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey()
{
    return "successfulSpeculativeWarmupWithRevalidation"_s;
}

String DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey()
{
    return "successfulSpeculativeWarmupWithoutRevalidation"_s;
}

String DiagnosticLoggingKeys::svgDocumentKey()
{
    return "svgDocument"_s;
}

String DiagnosticLoggingKeys::synchronousMessageFailedKey()
{
    return "synchronousMessageFailed"_s;
}

String DiagnosticLoggingKeys::telemetryPageLoadKey()
{
    return "telemetryPageLoad"_s;
}

String DiagnosticLoggingKeys::timedOutKey()
{
    return "timedOut"_s;
}

String DiagnosticLoggingKeys::canceledLessThan2SecondsKey()
{
    return "canceledLessThan2Seconds"_s;
}

String DiagnosticLoggingKeys::canceledLessThan5SecondsKey()
{
    return "canceledLessThan5Seconds"_s;
}

String DiagnosticLoggingKeys::canceledLessThan20SecondsKey()
{
    return "canceledLessThan20Seconds"_s;
}

String DiagnosticLoggingKeys::canceledMoreThan20SecondsKey()
{
    return "canceledMoreThan20Seconds"_s;
}

String DiagnosticLoggingKeys::failedLessThan2SecondsKey()
{
    return "failedLessThan2Seconds"_s;
}

String DiagnosticLoggingKeys::failedLessThan5SecondsKey()
{
    return "failedLessThan5Seconds"_s;
}

String DiagnosticLoggingKeys::failedLessThan20SecondsKey()
{
    return "failedLessThan20Seconds"_s;
}

String DiagnosticLoggingKeys::failedMoreThan20SecondsKey()
{
    return "failedMoreThan20Seconds"_s;
}

String DiagnosticLoggingKeys::occurredKey()
{
    return "occurred"_s;
}

String DiagnosticLoggingKeys::succeededLessThan2SecondsKey()
{
    return "succeededLessThan2Seconds"_s;
}

String DiagnosticLoggingKeys::succeededLessThan5SecondsKey()
{
    return "succeededLessThan5Seconds"_s;
}

String DiagnosticLoggingKeys::succeededLessThan20SecondsKey()
{
    return "succeededLessThan20Seconds"_s;
}

String DiagnosticLoggingKeys::succeededMoreThan20SecondsKey()
{
    return "succeededMoreThan20Seconds"_s;
}

String DiagnosticLoggingKeys::uncacheableStatusCodeKey()
{
    return "uncacheableStatusCode"_s;
}

String DiagnosticLoggingKeys::underMemoryPressureKey()
{
    return "underMemoryPressure"_s;
}

String DiagnosticLoggingKeys::unknownEntryRequestKey()
{
    return "unknownEntryRequest"_s;
}

String DiagnosticLoggingKeys::unlikelyToReuseKey()
{
    return "unlikelyToReuse"_s;
}

String DiagnosticLoggingKeys::unsupportedHTTPMethodKey()
{
    return "unsupportedHTTPMethod"_s;
}

String DiagnosticLoggingKeys::unsuspendableDOMObjectKey()
{
    return "unsuspendableDOMObject"_s;
}

String DiagnosticLoggingKeys::unusedKey()
{
    return "unused"_s;
}

String DiagnosticLoggingKeys::unusedReasonCredentialSettingsKey()
{
    return "unused.reason.credentialSettings"_s;
}

String DiagnosticLoggingKeys::unusedReasonErrorKey()
{
    return "unused.reason.error"_s;
}

String DiagnosticLoggingKeys::unusedReasonMustRevalidateNoValidatorKey()
{
    return "unused.reason.mustRevalidateNoValidator"_s;
}

String DiagnosticLoggingKeys::unusedReasonNoStoreKey()
{
    return "unused.reason.noStore"_s;
}

String DiagnosticLoggingKeys::unusedReasonRedirectChainKey()
{
    return "unused.reason.redirectChain"_s;
}

String DiagnosticLoggingKeys::unusedReasonReloadKey()
{
    return "unused.reason.reload"_s;
}

String DiagnosticLoggingKeys::unusedReasonTypeMismatchKey()
{
    return "unused.reason.typeMismatch"_s;
}

String DiagnosticLoggingKeys::usedKey()
{
    return "used"_s;
}

String DiagnosticLoggingKeys::userZoomActionKey()
{
    return "userZoomAction"_s;
}

String DiagnosticLoggingKeys::varyingHeaderMismatchKey()
{
    return "varyingHeaderMismatch"_s;
}

String DiagnosticLoggingKeys::videoKey()
{
    return "video"_s;
}

String DiagnosticLoggingKeys::visibleNonActiveStateKey()
{
    return "visibleNonActiveState"_s;
}

String DiagnosticLoggingKeys::visibleAndActiveStateKey()
{
    return "visibleAndActiveState"_s;
}

String DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey()
{
    return "wastedSpeculativeWarmupWithRevalidation"_s;
}

String DiagnosticLoggingKeys::wastedSpeculativeWarmupWithoutRevalidationKey()
{
    return "wastedSpeculativeWarmupWithoutRevalidation"_s;
}

String DiagnosticLoggingKeys::webViewKey()
{
    return "webView"_s;
}

String DiagnosticLoggingKeys::yesKey()
{
    return "yes"_s;
}

String DiagnosticLoggingKeys::expiredKey()
{
    return "expired"_s;
}

String DiagnosticLoggingKeys::fontKey()
{
    return "font"_s;
}

String DiagnosticLoggingKeys::prunedDueToMemoryPressureKey()
{
    return "pruned.memoryPressure"_s;
}

String DiagnosticLoggingKeys::prunedDueToMaxSizeReached()
{
    return "pruned.capacityReached"_s;
}

String DiagnosticLoggingKeys::prunedDueToProcessSuspended()
{
    return "pruned.processSuspended"_s;
}

String WebCore::DiagnosticLoggingKeys::notHTTPFamilyKey()
{
    return "notHTTPFamily"_s;
}

String WebCore::DiagnosticLoggingKeys::webGLStateKey()
{
    return "webGLState"_s;
}

String DiagnosticLoggingKeys::memoryUsageToDiagnosticLoggingKey(uint64_t memoryUsage)
{
    if (memoryUsage < 32 * MB)
        return "below32"_s;
    if (memoryUsage < 64 * MB)
        return "32to64"_s;
    if (memoryUsage < 128 * MB)
        return "64to128"_s;
    if (memoryUsage < 256 * MB)
        return "128to256"_s;
    if (memoryUsage < 512 * MB)
        return "256to512"_s;
    if (memoryUsage < 1024 * MB)
        return "512to1024"_s;
    if (memoryUsage < 2048 * MB)
        return "1024to2048"_s;
    if (memoryUsage < 4096llu * MB)
        return "2048to4096"_s;
    if (memoryUsage < 8192llu * MB)
        return "4096to8192"_s;
    if (memoryUsage < 16384llu * MB)
        return "8192to16384"_s;
    if (memoryUsage < 32768llu * MB)
        return "16384to32768"_s;
    return "over32768"_s;
}

String DiagnosticLoggingKeys::foregroundCPUUsageToDiagnosticLoggingKey(double cpuUsage)
{
    if (cpuUsage < 10)
        return "below10"_s;
    if (cpuUsage < 20)
        return "10to20"_s;
    if (cpuUsage < 40)
        return "20to40"_s;
    if (cpuUsage < 60)
        return "40to60"_s;
    if (cpuUsage < 80)
        return "60to80"_s;
    return "over80"_s;
}

String DiagnosticLoggingKeys::backgroundCPUUsageToDiagnosticLoggingKey(double cpuUsage)
{
    if (cpuUsage < 1)
        return "below1"_s;
    if (cpuUsage < 5)
        return "1to5"_s;
    if (cpuUsage < 10)
        return "5to10"_s;
    if (cpuUsage < 30)
        return "10to30"_s;
    if (cpuUsage < 50)
        return "30to50"_s;
    if (cpuUsage < 70)
        return "50to70"_s;
    return "over70"_s;
}

String DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey()
{
    return "resourceLoadStatisticsTelemetry"_s;
}
    
} // namespace WebCore

