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

String DiagnosticLoggingKeys::pluginLoadedKey()
{
    return ASCIILiteral("pluginLoaded");
}

String DiagnosticLoggingKeys::pluginLoadingFailedKey()
{
    return ASCIILiteral("pluginFailedLoading");
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

String DiagnosticLoggingKeys::engineFailedToLoadKey()
{
    return ASCIILiteral("engineFailedToLoad");
}

String DiagnosticLoggingKeys::navigationKey()
{
    return ASCIILiteral("navigation");
}

String DiagnosticLoggingKeys::pageCacheKey()
{
    return ASCIILiteral("pageCache");
}

String DiagnosticLoggingKeys::noDocumentLoaderKey()
{
    return ASCIILiteral("noDocumentLoader");
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

String DiagnosticLoggingKeys::loadedKey()
{
    return ASCIILiteral("loaded");
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

String DiagnosticLoggingKeys::hasOpenDatabasesKey()
{
    return ASCIILiteral("hasOpenDatabases");
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

String DiagnosticLoggingKeys::loadingAPISenseKey()
{
    return ASCIILiteral("loadingAPISense");
}

String DiagnosticLoggingKeys::documentLoaderStoppingKey()
{
    return ASCIILiteral("documentLoaderStopping");
}

String DiagnosticLoggingKeys::cannotSuspendActiveDOMObjectsKey()
{
    return ASCIILiteral("cannotSuspendActiveDOMObjects");
}

String DiagnosticLoggingKeys::applicationCacheKey()
{
    return ASCIILiteral("applicationCache");
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

String DiagnosticLoggingKeys::reloadKey()
{
    return ASCIILiteral("reload");
}

String DiagnosticLoggingKeys::resourceKey()
{
    return ASCIILiteral("resource");
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

String DiagnosticLoggingKeys::styleSheetKey()
{
    return ASCIILiteral("styleSheet");
}

String DiagnosticLoggingKeys::svgDocumentKey()
{
    return ASCIILiteral("svgDocument");
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

String DiagnosticLoggingKeys::prunedDueToCapacityReached()
{
    return ASCIILiteral("pruned.capacityReached");
}

String DiagnosticLoggingKeys::prunedDueToProcessSuspended()
{
    return ASCIILiteral("pruned.processSuspended");
}

} // namespace WebCore

