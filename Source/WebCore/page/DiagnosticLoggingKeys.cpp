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

#include "config.h"
#include "DiagnosticLoggingKeys.h"

namespace WebCore {

String DiagnosticLoggingKeys::mediaLoadedKey()
{
    return ASCIILiteral("mediaLoaded");
}

String DiagnosticLoggingKeys::mediaLoadingFailedKey()
{
    return ASCIILiteral("mediaFailedLoading");
}

String DiagnosticLoggingKeys::memoryCacheEntryDecisionKey()
{
    return ASCIILiteral("memoryCacheEntryDecision");
}

String DiagnosticLoggingKeys::memoryCacheUsageKey()
{
    return ASCIILiteral("memoryCacheUsage");
}

String DiagnosticLoggingKeys::missingValidatorFieldsKey()
{
    return ASCIILiteral("missingValidatorFields");
}

String DiagnosticLoggingKeys::pluginLoadedKey()
{
    return ASCIILiteral("pluginLoaded");
}

String DiagnosticLoggingKeys::pluginLoadingFailedKey()
{
    return ASCIILiteral("pluginFailedLoading");
}

String DiagnosticLoggingKeys::postPageBackgroundingCPUUsageKey()
{
    return ASCIILiteral("postPageBackgroundingCPUUsage");
}

String DiagnosticLoggingKeys::postPageBackgroundingMemoryUsageKey()
{
    return ASCIILiteral("postPageBackgroundingMemoryUsage");
}

String DiagnosticLoggingKeys::pageHandlesWebGLContextLossKey()
{
    return ASCIILiteral("pageHandlesWebGLContextLoss");
}

String DiagnosticLoggingKeys::postPageLoadCPUUsageKey()
{
    return ASCIILiteral("postPageLoadCPUUsage");
}

String DiagnosticLoggingKeys::postPageLoadMemoryUsageKey()
{
    return ASCIILiteral("postPageLoadMemoryUsage");
}

String DiagnosticLoggingKeys::provisionalLoadKey()
{
    return ASCIILiteral("provisionalLoad");
}

String DiagnosticLoggingKeys::pageContainsPluginKey()
{
    return ASCIILiteral("pageContainsPlugin");
}

String DiagnosticLoggingKeys::pageContainsAtLeastOnePluginKey()
{
    return ASCIILiteral("pageContainsAtLeastOnePlugin");
}

String DiagnosticLoggingKeys::pageContainsMediaEngineKey()
{
    return ASCIILiteral("pageContainsMediaEngine");
}

String DiagnosticLoggingKeys::pageContainsAtLeastOneMediaEngineKey()
{
    return ASCIILiteral("pageContainsAtLeastOneMediaEngine");
}

String DiagnosticLoggingKeys::pageLoadedKey()
{
    return ASCIILiteral("pageLoaded");
}

String DiagnosticLoggingKeys::playedKey()
{
    return ASCIILiteral("played");
}

String DiagnosticLoggingKeys::engineFailedToLoadKey()
{
    return ASCIILiteral("engineFailedToLoad");
}

String DiagnosticLoggingKeys::entryRightlyNotWarmedUpKey()
{
    return ASCIILiteral("entryRightlyNotWarmedUp");
}

String DiagnosticLoggingKeys::entryWronglyNotWarmedUpKey()
{
    return ASCIILiteral("entryWronglyNotWarmedUp");
}

String DiagnosticLoggingKeys::navigationKey()
{
    return ASCIILiteral("navigation");
}

String DiagnosticLoggingKeys::needsRevalidationKey()
{
    return ASCIILiteral("needsRevalidation");
}

String DiagnosticLoggingKeys::networkCacheKey()
{
    return ASCIILiteral("networkCache");
}

String DiagnosticLoggingKeys::networkCacheFailureReasonKey()
{
    return ASCIILiteral("networkCacheFailureReason");
}

String DiagnosticLoggingKeys::networkCacheUnusedReasonKey()
{
    return ASCIILiteral("networkCacheUnusedReason");
}

String DiagnosticLoggingKeys::networkCacheReuseFailureKey()
{
    return ASCIILiteral("networkCacheReuseFailure");
}

String DiagnosticLoggingKeys::networkKey()
{
    return ASCIILiteral("network");
}

String DiagnosticLoggingKeys::networkProcessCrashedKey()
{
    return ASCIILiteral("networkProcessCrashed");
}

String DiagnosticLoggingKeys::neverSeenBeforeKey()
{
    return ASCIILiteral("neverSeenBefore");
}

String DiagnosticLoggingKeys::noKey()
{
    return ASCIILiteral("no");
}

String DiagnosticLoggingKeys::noCacheKey()
{
    return ASCIILiteral("noCache");
}

String DiagnosticLoggingKeys::noStoreKey()
{
    return ASCIILiteral("noStore");
}

String DiagnosticLoggingKeys::nonVisibleStateKey()
{
    return ASCIILiteral("nonVisibleState");
}

String DiagnosticLoggingKeys::notInMemoryCacheKey()
{
    return ASCIILiteral("notInMemoryCache");
}

String DiagnosticLoggingKeys::pageCacheKey()
{
    return ASCIILiteral("pageCache");
}

String DiagnosticLoggingKeys::pageCacheFailureKey()
{
    return ASCIILiteral("pageCacheFailure");
}

String DiagnosticLoggingKeys::noDocumentLoaderKey()
{
    return ASCIILiteral("noDocumentLoader");
}

String DiagnosticLoggingKeys::noLongerInCacheKey()
{
    return ASCIILiteral("noLongerInCache");
}

String DiagnosticLoggingKeys::otherKey()
{
    return ASCIILiteral("other");
}

String DiagnosticLoggingKeys::mainDocumentErrorKey()
{
    return ASCIILiteral("mainDocumentError");
}

String DiagnosticLoggingKeys::mainResourceKey()
{
    return ASCIILiteral("mainResource");
}

String DiagnosticLoggingKeys::isErrorPageKey()
{
    return ASCIILiteral("isErrorPage");
}

String DiagnosticLoggingKeys::isExpiredKey()
{
    return ASCIILiteral("isExpired");
}

String DiagnosticLoggingKeys::isReloadIgnoringCacheDataKey()
{
    return ASCIILiteral("isReloadIgnoringCacheData");
}

String DiagnosticLoggingKeys::loadingKey()
{
    return ASCIILiteral("loading");
}

String DiagnosticLoggingKeys::hasPluginsKey()
{
    return ASCIILiteral("hasPlugins");
}

String DiagnosticLoggingKeys::httpsNoStoreKey()
{
    return ASCIILiteral("httpsNoStore");
}

String DiagnosticLoggingKeys::imageKey()
{
    return ASCIILiteral("image");
}

String DiagnosticLoggingKeys::inMemoryCacheKey()
{
    return ASCIILiteral("inMemoryCache");
}

String DiagnosticLoggingKeys::inactiveKey()
{
    return ASCIILiteral("inactive");
}

String DiagnosticLoggingKeys::internalErrorKey()
{
    return ASCIILiteral("internalError");
}

String DiagnosticLoggingKeys::invalidSessionIDKey()
{
    return ASCIILiteral("invalidSessionID");
}

String DiagnosticLoggingKeys::isAttachmentKey()
{
    return ASCIILiteral("isAttachment");
}

String DiagnosticLoggingKeys::isConditionalRequestKey()
{
    return ASCIILiteral("isConditionalRequest");
}

String DiagnosticLoggingKeys::isDisabledKey()
{
    return ASCIILiteral("isDisabled");
}

String DiagnosticLoggingKeys::noCurrentHistoryItemKey()
{
    return ASCIILiteral("noCurrentHistoryItem");
}

String DiagnosticLoggingKeys::quirkRedirectComingKey()
{
    return ASCIILiteral("quirkRedirectComing");
}

String DiagnosticLoggingKeys::rawKey()
{
    return ASCIILiteral("raw");
}

String DiagnosticLoggingKeys::redirectKey()
{
    return ASCIILiteral("redirect");
}

String DiagnosticLoggingKeys::isLoadingKey()
{
    return ASCIILiteral("isLoading");
}

String DiagnosticLoggingKeys::documentLoaderStoppingKey()
{
    return ASCIILiteral("documentLoaderStopping");
}

String DiagnosticLoggingKeys::domainCausingEnergyDrainKey()
{
    return ASCIILiteral("DomainCausingEnergyDrain");
}

String DiagnosticLoggingKeys::domainCausingJetsamKey()
{
    return ASCIILiteral("DomainCausingJetsam");
}

String DiagnosticLoggingKeys::simulatedPageCrashKey()
{
    return ASCIILiteral("SimulatedPageCrash");
}

String DiagnosticLoggingKeys::exceededActiveMemoryLimitKey()
{
    return ASCIILiteral("ExceededActiveMemoryLimit");
}

String DiagnosticLoggingKeys::exceededInactiveMemoryLimitKey()
{
    return ASCIILiteral("ExceededInactiveMemoryLimit");
}

String DiagnosticLoggingKeys::exceededBackgroundCPULimitKey()
{
    return ASCIILiteral("ExceededBackgroundCPULimit");
}

String DiagnosticLoggingKeys::domainVisitedKey()
{
    return ASCIILiteral("DomainVisited");
}

String DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey()
{
    return ASCIILiteral("cannotSuspendActiveDOMObjects");
}

String DiagnosticLoggingKeys::cpuUsageKey()
{
    return ASCIILiteral("cpuUsage");
}

String DiagnosticLoggingKeys::createSharedBufferFailedKey()
{
    return ASCIILiteral("createSharedBufferFailed");
}

String DiagnosticLoggingKeys::activeInForegroundTabKey()
{
    return ASCIILiteral("activeInForegroundTab");
}

String DiagnosticLoggingKeys::activeInBackgroundTabOnlyKey()
{
    return ASCIILiteral("activeInBackgroundTabOnly");
}

String DiagnosticLoggingKeys::applicationCacheKey()
{
    return ASCIILiteral("applicationCache");
}

String DiagnosticLoggingKeys::audioKey()
{
    return ASCIILiteral("audio");
}

String DiagnosticLoggingKeys::backNavigationDeltaKey()
{
    return ASCIILiteral("backNavigationDelta");
}

String DiagnosticLoggingKeys::canCacheKey()
{
    return ASCIILiteral("canCache");
}

String DiagnosticLoggingKeys::cacheControlNoStoreKey()
{
    return ASCIILiteral("cacheControlNoStore");
}

String DiagnosticLoggingKeys::cachedResourceRevalidationKey()
{
    return ASCIILiteral("cachedResourceRevalidation");
}

String DiagnosticLoggingKeys::cachedResourceRevalidationReasonKey()
{
    return ASCIILiteral("cachedResourceRevalidationReason");
}

String DiagnosticLoggingKeys::deniedByClientKey()
{
    return ASCIILiteral("deniedByClient");
}

String DiagnosticLoggingKeys::deviceMotionKey()
{
    return ASCIILiteral("deviceMotion");
}

String DiagnosticLoggingKeys::deviceOrientationKey()
{
    return ASCIILiteral("deviceOrientation");
}

String DiagnosticLoggingKeys::diskCacheKey()
{
    return ASCIILiteral("diskCache");
}

String DiagnosticLoggingKeys::diskCacheAfterValidationKey()
{
    return ASCIILiteral("diskCacheAfterValidation");
}

String DiagnosticLoggingKeys::reloadKey()
{
    return ASCIILiteral("reload");
}

String DiagnosticLoggingKeys::replaceKey()
{
    return ASCIILiteral("replace");
}

String DiagnosticLoggingKeys::retrievalRequestKey()
{
    return ASCIILiteral("retrievalRequest");
}

String DiagnosticLoggingKeys::resourceLoadedKey()
{
    return ASCIILiteral("resourceLoaded");
}

String DiagnosticLoggingKeys::resourceResponseSourceKey()
{
    return ASCIILiteral("resourceResponseSource");
}

String DiagnosticLoggingKeys::retrievalKey()
{
    return ASCIILiteral("retrieval");
}

String DiagnosticLoggingKeys::revalidatingKey()
{
    return ASCIILiteral("revalidating");
}

String DiagnosticLoggingKeys::reloadFromOriginKey()
{
    return ASCIILiteral("reloadFromOrigin");
}

String DiagnosticLoggingKeys::reloadRevalidatingExpiredKey()
{
    return ASCIILiteral("reloadRevalidatingExpired");
}

String DiagnosticLoggingKeys::sameLoadKey()
{
    return ASCIILiteral("sameLoad");
}

String DiagnosticLoggingKeys::scriptKey()
{
    return ASCIILiteral("script");
}

String DiagnosticLoggingKeys::streamingMedia()
{
    return ASCIILiteral("streamingMedia");
}

String DiagnosticLoggingKeys::styleSheetKey()
{
    return ASCIILiteral("styleSheet");
}

String DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey()
{
    return ASCIILiteral("successfulSpeculativeWarmupWithRevalidation");
}

String DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey()
{
    return ASCIILiteral("successfulSpeculativeWarmupWithoutRevalidation");
}

String DiagnosticLoggingKeys::svgDocumentKey()
{
    return ASCIILiteral("svgDocument");
}

String DiagnosticLoggingKeys::synchronousMessageFailedKey()
{
    return ASCIILiteral("synchronousMessageFailed");
}

String DiagnosticLoggingKeys::uncacheableStatusCodeKey()
{
    return ASCIILiteral("uncacheableStatusCode");
}

String DiagnosticLoggingKeys::underMemoryPressureKey()
{
    return ASCIILiteral("underMemoryPressure");
}

String DiagnosticLoggingKeys::unknownEntryRequestKey()
{
    return ASCIILiteral("unknownEntryRequest");
}

String DiagnosticLoggingKeys::unlikelyToReuseKey()
{
    return ASCIILiteral("unlikelyToReuse");
}

String DiagnosticLoggingKeys::unsupportedHTTPMethodKey()
{
    return ASCIILiteral("unsupportedHTTPMethod");
}

String DiagnosticLoggingKeys::unsuspendableDOMObjectKey()
{
    return ASCIILiteral("unsuspendableDOMObject");
}

String DiagnosticLoggingKeys::unusedKey()
{
    return ASCIILiteral("unused");
}

String DiagnosticLoggingKeys::unusedReasonCredentialSettingsKey()
{
    return ASCIILiteral("unused.reason.credentialSettings");
}

String DiagnosticLoggingKeys::unusedReasonErrorKey()
{
    return ASCIILiteral("unused.reason.error");
}

String DiagnosticLoggingKeys::unusedReasonMustRevalidateNoValidatorKey()
{
    return ASCIILiteral("unused.reason.mustRevalidateNoValidator");
}

String DiagnosticLoggingKeys::unusedReasonNoStoreKey()
{
    return ASCIILiteral("unused.reason.noStore");
}

String DiagnosticLoggingKeys::unusedReasonRedirectChainKey()
{
    return ASCIILiteral("unused.reason.redirectChain");
}

String DiagnosticLoggingKeys::unusedReasonReloadKey()
{
    return ASCIILiteral("unused.reason.reload");
}

String DiagnosticLoggingKeys::unusedReasonTypeMismatchKey()
{
    return ASCIILiteral("unused.reason.typeMismatch");
}

String DiagnosticLoggingKeys::usedKey()
{
    return ASCIILiteral("used");
}

String DiagnosticLoggingKeys::userZoomActionKey()
{
    return ASCIILiteral("userZoomAction");
}

String DiagnosticLoggingKeys::varyingHeaderMismatchKey()
{
    return ASCIILiteral("varyingHeaderMismatch");
}

String DiagnosticLoggingKeys::videoKey()
{
    return ASCIILiteral("video");
}

String DiagnosticLoggingKeys::visibleNonActiveStateKey()
{
    return ASCIILiteral("visibleNonActiveState");
}

String DiagnosticLoggingKeys::visibleAndActiveStateKey()
{
    return ASCIILiteral("visibleAndActiveState");
}

String DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey()
{
    return ASCIILiteral("wastedSpeculativeWarmupWithRevalidation");
}

String DiagnosticLoggingKeys::wastedSpeculativeWarmupWithoutRevalidationKey()
{
    return ASCIILiteral("wastedSpeculativeWarmupWithoutRevalidation");
}

String DiagnosticLoggingKeys::webViewKey()
{
    return ASCIILiteral("webView");
}

String DiagnosticLoggingKeys::yesKey()
{
    return ASCIILiteral("yes");
}

String DiagnosticLoggingKeys::expiredKey()
{
    return ASCIILiteral("expired");
}

String DiagnosticLoggingKeys::fontKey()
{
    return ASCIILiteral("font");
}

String DiagnosticLoggingKeys::prunedDueToMemoryPressureKey()
{
    return ASCIILiteral("pruned.memoryPressure");
}

String DiagnosticLoggingKeys::prunedDueToMaxSizeReached()
{
    return ASCIILiteral("pruned.capacityReached");
}

String DiagnosticLoggingKeys::prunedDueToProcessSuspended()
{
    return ASCIILiteral("pruned.processSuspended");
}

String WebCore::DiagnosticLoggingKeys::notHTTPFamilyKey()
{
    return ASCIILiteral("notHTTPFamily");
}

String WebCore::DiagnosticLoggingKeys::webGLStateKey()
{
    return ASCIILiteral("webGLState");
}

String DiagnosticLoggingKeys::memoryUsageToDiagnosticLoggingKey(uint64_t memoryUsage)
{
    if (memoryUsage < 32 * MB)
        return ASCIILiteral("below32");
    if (memoryUsage < 64 * MB)
        return ASCIILiteral("32to64");
    if (memoryUsage < 128 * MB)
        return ASCIILiteral("64to128");
    if (memoryUsage < 256 * MB)
        return ASCIILiteral("128to256");
    if (memoryUsage < 512 * MB)
        return ASCIILiteral("256to512");
    if (memoryUsage < 1024 * MB)
        return ASCIILiteral("512to1024");
    if (memoryUsage < 2048 * MB)
        return ASCIILiteral("1024to2048");
    if (memoryUsage < 4096llu * MB)
        return ASCIILiteral("2048to4096");
    if (memoryUsage < 8192llu * MB)
        return ASCIILiteral("4096to8192");
    if (memoryUsage < 16384llu * MB)
        return ASCIILiteral("8192to16384");
    if (memoryUsage < 32768llu * MB)
        return ASCIILiteral("16384to32768");
    return ASCIILiteral("over32768");
}

String DiagnosticLoggingKeys::foregroundCPUUsageToDiagnosticLoggingKey(double cpuUsage)
{
    if (cpuUsage < 10)
        return ASCIILiteral("below10");
    if (cpuUsage < 20)
        return ASCIILiteral("10to20");
    if (cpuUsage < 40)
        return ASCIILiteral("20to40");
    if (cpuUsage < 60)
        return ASCIILiteral("40to60");
    if (cpuUsage < 80)
        return ASCIILiteral("60to80");
    return ASCIILiteral("over80");
}

String DiagnosticLoggingKeys::backgroundCPUUsageToDiagnosticLoggingKey(double cpuUsage)
{
    if (cpuUsage < 1)
        return ASCIILiteral("below1");
    if (cpuUsage < 5)
        return ASCIILiteral("1to5");
    if (cpuUsage < 10)
        return ASCIILiteral("5to10");
    if (cpuUsage < 30)
        return ASCIILiteral("10to30");
    if (cpuUsage < 50)
        return ASCIILiteral("30to50");
    if (cpuUsage < 70)
        return ASCIILiteral("50to70");
    return ASCIILiteral("over70");
}

String DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey()
{
    return ASCIILiteral("resourceLoadStatisticsTelemetry");
}
    
} // namespace WebCore

