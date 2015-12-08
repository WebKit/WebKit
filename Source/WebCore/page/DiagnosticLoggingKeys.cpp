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

String DiagnosticLoggingKeys::successKey()
{
    return ASCIILiteral("success");
}

String DiagnosticLoggingKeys::failureKey()
{
    return ASCIILiteral("failure");
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

String DiagnosticLoggingKeys::networkKey()
{
    return ASCIILiteral("network");
}

String DiagnosticLoggingKeys::neverSeenBeforeKey()
{
    return ASCIILiteral("neverSeenBefore");
}

String DiagnosticLoggingKeys::noCacheKey()
{
    return ASCIILiteral("noCache");
}

String DiagnosticLoggingKeys::noStoreKey()
{
    return ASCIILiteral("noStore");
}

String DiagnosticLoggingKeys::notInMemoryCacheKey()
{
    return ASCIILiteral("notInMemoryCache");
}

String DiagnosticLoggingKeys::pageCacheKey()
{
    return ASCIILiteral("pageCache");
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

String DiagnosticLoggingKeys::mediaKey()
{
    return ASCIILiteral("media");
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

String DiagnosticLoggingKeys::loadedKey()
{
    return ASCIILiteral("loaded");
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

String DiagnosticLoggingKeys::reasonKey()
{
    return ASCIILiteral("reason");
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

String DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey()
{
    return ASCIILiteral("cannotSuspendActiveDOMObjects");
}

String DiagnosticLoggingKeys::deltaKey()
{
    return ASCIILiteral("delta");
}

String DiagnosticLoggingKeys::applicationCacheKey()
{
    return ASCIILiteral("applicationCache");
}

String DiagnosticLoggingKeys::audioKey()
{
    return ASCIILiteral("audio");
}

String DiagnosticLoggingKeys::backNavigationKey()
{
    return ASCIILiteral("backNavigation");
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

String DiagnosticLoggingKeys::deviceProximityKey()
{
    return ASCIILiteral("deviceProximity");
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

String DiagnosticLoggingKeys::requestKey()
{
    return ASCIILiteral("request");
}

String DiagnosticLoggingKeys::retrievalRequestKey()
{
    return ASCIILiteral("retrievalRequest");
}

String DiagnosticLoggingKeys::resourceKey()
{
    return ASCIILiteral("resource");
}

String DiagnosticLoggingKeys::resourceRequestKey()
{
    return ASCIILiteral("resourceRequest");
}

String DiagnosticLoggingKeys::resourceResponseKey()
{
    return ASCIILiteral("resourceResponse");
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

String DiagnosticLoggingKeys::sameLoadKey()
{
    return ASCIILiteral("sameLoad");
}

String DiagnosticLoggingKeys::scriptKey()
{
    return ASCIILiteral("script");
}

String DiagnosticLoggingKeys::sourceKey()
{
    return ASCIILiteral("source");
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

String DiagnosticLoggingKeys::unusableCachedEntryKey()
{
    return ASCIILiteral("unusableCachedEntry");
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

String DiagnosticLoggingKeys::userKey()
{
    return ASCIILiteral("user");
}

String DiagnosticLoggingKeys::varyingHeaderMismatchKey()
{
    return ASCIILiteral("varyingHeaderMismatch");
}

String DiagnosticLoggingKeys::videoKey()
{
    return ASCIILiteral("video");
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

String DiagnosticLoggingKeys::zoomedKey()
{
    return ASCIILiteral("zoomed");
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

String DiagnosticLoggingKeys::notInCacheKey()
{
    return ASCIILiteral("notInCache");
}

} // namespace WebCore

