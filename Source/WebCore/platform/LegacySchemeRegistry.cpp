/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "config.h"
#include "LegacySchemeRegistry.h"

#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/URLParser.h>

#if ENABLE(CONTENT_FILTERING)
#include "ContentFilter.h"
#endif
#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

namespace WebCore {

// FIXME: URLSchemesMap is a peculiar type name given that it is a set.

static const URLSchemesMap& builtinLocalURLSchemes();
static std::span<const ASCIILiteral> builtinSecureSchemes();
static std::span<const ASCIILiteral> builtinSchemesWithUniqueOrigins();
static std::span<const ASCIILiteral> builtinEmptyDocumentSchemes();
static std::span<const ASCIILiteral> builtinCanDisplayOnlyIfCanRequestSchemes();
static std::span<const ASCIILiteral> builtinCORSEnabledSchemes();

using ASCIILiteralSpanFunction = std::span<const ASCIILiteral> (*)();

static void add(URLSchemesMap& set, ASCIILiteralSpanFunction function)
{
    for (auto& scheme : function())
        set.add(scheme);
}

static NeverDestroyed<URLSchemesMap> makeNeverDestroyedSchemeSet(ASCIILiteralSpanFunction function)
{
    URLSchemesMap set;
    add(set, function);
    return set;
}

static Lock schemeRegistryLock;

static const URLSchemesMap& allBuiltinSchemes()
{
    static NeverDestroyed schemes = [] {
        static constexpr ASCIILiteralSpanFunction functions[] {
            builtinSecureSchemes,
            builtinSchemesWithUniqueOrigins,
            builtinEmptyDocumentSchemes,
            builtinCanDisplayOnlyIfCanRequestSchemes,
            builtinCORSEnabledSchemes,
        };

        // Other misc schemes that the LegacySchemeRegistry doesn't know about.
        static constexpr auto otherSchemes = WTF::toArray<ASCIILiteral>({
            "webkit-fake-url"_s,
#if PLATFORM(MAC)
            "safari-extension"_s,
#endif
#if USE(QUICK_LOOK)
            QLPreviewProtocol,
#endif
#if ENABLE(CONTENT_FILTERING)
            ContentFilter::urlScheme(),
#endif
        });

        URLSchemesMap set;
        {
            Locker locker { schemeRegistryLock };
            for (auto& scheme : builtinLocalURLSchemes())
                set.add(scheme);
        }
        for (auto& function : functions)
            add(set, function);
        for (auto& scheme : otherSchemes)
            set.add(scheme);
        return set;
    }();
    return schemes;
}

static const URLSchemesMap& builtinLocalURLSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed schemes = URLSchemesMap {
        "file"_s,
#if PLATFORM(COCOA)
        "applewebdata"_s,
#endif
    };
    return schemes;
}

static URLSchemesMap& localURLSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> localSchemes = builtinLocalURLSchemes();
    return localSchemes;
}

NODELETE static URLSchemesMap& displayIsolatedURLSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> displayIsolatedSchemes;
    return displayIsolatedSchemes;
}

static std::span<const ASCIILiteral> builtinSecureSchemes()
{
    static constexpr std::array schemes {
        "https"_s,
        "about"_s,
        "data"_s,
        "wss"_s,
#if PLATFORM(GTK) || PLATFORM(WPE)
        "resource"_s,
#endif
#if ENABLE(PDFJS)
        "webkit-pdfjs-viewer"_s,
#endif
    };
    return schemes;
}

static URLSchemesMap& secureSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static auto secureSchemes = makeNeverDestroyedSchemeSet(builtinSecureSchemes);
    return secureSchemes;
}

NODELETE static URLSchemesMap& schemesAllowingServiceWorkerClients()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemesAllowingServiceWorkerClients;
    return schemesAllowingServiceWorkerClients;
}

static std::span<const ASCIILiteral> builtinSchemesWithUniqueOrigins()
{
    static constexpr std::array schemes {
        "about"_s,
        "javascript"_s,
        // This is an intentional difference from the behavior the HTML specification calls for.
        // See https://bugs.webkit.org/show_bug.cgi?id=11885
        "data"_s,
    };
    return schemes;
}

static URLSchemesMap& schemesWithUniqueOrigins() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static auto schemesWithUniqueOrigins = makeNeverDestroyedSchemeSet(builtinSchemesWithUniqueOrigins);
    return schemesWithUniqueOrigins;
}

static std::span<const ASCIILiteral> builtinEmptyDocumentSchemes()
{
    static constexpr std::array schemes { "about"_s };
    return schemes;
}

static URLSchemesMap& emptyDocumentSchemes()
{
    ASSERT(isMainThread());
    static auto emptyDocumentSchemes = makeNeverDestroyedSchemeSet(builtinEmptyDocumentSchemes);
    return emptyDocumentSchemes;
}

NODELETE static URLSchemesMap& schemesForbiddenFromDomainRelaxation()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

static std::span<const ASCIILiteral> builtinCanDisplayOnlyIfCanRequestSchemes()
{
    static constexpr std::array schemes { "blob"_s };
    return schemes;
}

static URLSchemesMap& canDisplayOnlyIfCanRequestSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(!isInNetworkProcess());
    ASSERT(schemeRegistryLock.isHeld());
    static auto canDisplayOnlyIfCanRequestSchemes = makeNeverDestroyedSchemeSet(builtinCanDisplayOnlyIfCanRequestSchemes);
    return canDisplayOnlyIfCanRequestSchemes;
}

NODELETE static URLSchemesMap& notAllowingJavascriptURLsSchemes()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> notAllowingJavascriptURLsSchemes;
    return notAllowingJavascriptURLsSchemes;
}

void LegacySchemeRegistry::registerURLSchemeAsLocal(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    localURLSchemes().add(scheme);
}

void LegacySchemeRegistry::removeURLSchemeRegisteredAsLocal(StringImpl* scheme)
{
    Locker locker { schemeRegistryLock };
    if (builtinLocalURLSchemes().contains(scheme))
        return;

    localURLSchemes().remove(scheme);
}

NODELETE static MemoryCompactRobinHoodHashSet<RefPtr<StringImpl>>& schemesHandledBySchemeHandler() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<MemoryCompactRobinHoodHashSet<RefPtr<StringImpl>>> set;
    return set.get();
}

void LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(StringImpl* scheme)
{
    if (equal(scheme, "about"_s))
        return;

    Locker locker { schemeRegistryLock };
    schemesHandledBySchemeHandler().add(scheme);
}

bool LegacySchemeRegistry::schemeIsHandledBySchemeHandler(StringView scheme)
{
    Locker locker { schemeRegistryLock };
    return schemesHandledBySchemeHandler().contains<StringViewHashTranslator>(scheme);
}

NODELETE static URLSchemesMap& schemesAllowingDatabaseAccessInPrivateBrowsing()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemesAllowingDatabaseAccessInPrivateBrowsing;
    return schemesAllowingDatabaseAccessInPrivateBrowsing;
}

static std::span<const ASCIILiteral> builtinCORSEnabledSchemes()
{
    static constexpr std::array schemes { "http"_s, "https"_s };
    return schemes;
}

static URLSchemesMap& CORSEnabledSchemes()
{
    ASSERT(isMainThread());
    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=77160
    static auto schemes = makeNeverDestroyedSchemeSet(builtinCORSEnabledSchemes);
    return schemes;
}

#if ENABLE(PDFJS)
static std::span<const ASCIILiteral> builtinCSPBypassingSchemes()
{
    static constexpr std::array schemes { "webkit-pdfjs-viewer"_s };
    return schemes;
}
#endif

NODELETE static URLSchemesMap& ContentSecurityPolicyBypassingSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
#if ENABLE(PDFJS)
    static auto schemes = makeNeverDestroyedSchemeSet(builtinCSPBypassingSchemes);
#else
    static NeverDestroyed<URLSchemesMap> schemes;
#endif
    return schemes;
}

NODELETE static URLSchemesMap& cachePartitioningSchemes() WTF_REQUIRES_LOCK(schemeRegistryLock)
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

NODELETE static URLSchemesMap& alwaysRevalidatedSchemes()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(StringView scheme)
{
    if (scheme.isNull())
        return false;

    Locker locker { schemeRegistryLock };
    return localURLSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsNoAccess(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    schemesWithUniqueOrigins().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsNoAccess(StringView scheme)
{
    if (scheme.isNull())
        return false;

    Locker locker { schemeRegistryLock };
    return schemesWithUniqueOrigins().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsDisplayIsolated(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    displayIsolatedURLSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(StringView scheme)
{
    if (scheme.isNull())
        return false;

    Locker locker { schemeRegistryLock };
    return displayIsolatedURLSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsSecure(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    secureSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(StringView scheme)
{
    if (scheme.isNull())
        return false;

    Locker locker { schemeRegistryLock };
    return secureSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsAllowingServiceWorkerClients(StringImpl* scheme)
{
    ASSERT(isMainThread());
    if (!scheme)
        return;

    schemesAllowingServiceWorkerClients().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsAllowingServiceWorkerClients(StringView scheme)
{
    ASSERT(isMainThread());
    if (scheme.isNull())
        return false;

    return schemesAllowingServiceWorkerClients().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsEmptyDocument(StringImpl* scheme)
{
    if (!scheme)
        return;
    emptyDocumentSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(StringView scheme)
{
    return !scheme.isNull() && emptyDocumentSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::setDomainRelaxationForbiddenForURLScheme(bool forbidden, StringImpl* scheme)
{
    if (!scheme)
        return;

    if (forbidden)
        schemesForbiddenFromDomainRelaxation().add(scheme);
    else
        schemesForbiddenFromDomainRelaxation().remove(scheme);
}

bool LegacySchemeRegistry::isDomainRelaxationForbiddenForURLScheme(StringImpl* scheme)
{
    return !!scheme && schemesForbiddenFromDomainRelaxation().contains(scheme);
}

bool LegacySchemeRegistry::canDisplayOnlyIfCanRequest(StringView scheme)
{
    ASSERT(!isInNetworkProcess());
    if (scheme.isNull())
        return false;

    Locker locker { schemeRegistryLock };
    return canDisplayOnlyIfCanRequestSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(StringImpl* scheme)
{
    ASSERT(!isInNetworkProcess());
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    canDisplayOnlyIfCanRequestSchemes().add(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(StringImpl* scheme)
{
    if (!scheme)
        return;
    notAllowingJavascriptURLsSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(StringImpl* scheme)
{
    return !!scheme && notAllowingJavascriptURLsSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(StringImpl* scheme)
{
    if (!scheme)
        return;
    schemesAllowingDatabaseAccessInPrivateBrowsing().add(scheme);
}

bool LegacySchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(StringImpl* scheme)
{
    return !!scheme && schemesAllowingDatabaseAccessInPrivateBrowsing().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsCORSEnabled(StringImpl* scheme)
{
    if (equal(scheme, "about"_s))
        return;

    ASSERT(!isInNetworkProcess());
    if (!scheme)
        return;
    CORSEnabledSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(StringView scheme)
{
    ASSERT(!isInNetworkProcess());
    return !scheme.isNull() && CORSEnabledSchemes().contains<StringViewHashTranslator>(scheme);
}

Vector<RefPtr<StringImpl>> LegacySchemeRegistry::allURLSchemesRegisteredAsCORSEnabled()
{
    ASSERT(!isInNetworkProcess());
    return copyToVector(CORSEnabledSchemes());
}

void LegacySchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    ContentSecurityPolicyBypassingSchemes().add(scheme);
}

void LegacySchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    ContentSecurityPolicyBypassingSchemes().remove(scheme);
}

bool LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(StringView scheme)
{
    if (scheme.isNull())
        return false;

    Locker locker { schemeRegistryLock };
    return ContentSecurityPolicyBypassingSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsAlwaysRevalidated(StringImpl* scheme)
{
    if (!scheme)
        return;
    alwaysRevalidatedSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldAlwaysRevalidateURLScheme(StringView scheme)
{
    return !scheme.isNull() && alwaysRevalidatedSchemes().contains<StringViewHashTranslator>(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsCachePartitioned(StringImpl* scheme)
{
    if (!scheme)
        return;

    Locker locker { schemeRegistryLock };
    cachePartitioningSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldPartitionCacheForURLScheme(StringImpl* scheme)
{
    if (!scheme)
        return false;

    Locker locker { schemeRegistryLock };
    return cachePartitioningSchemes().contains(scheme);
}

bool LegacySchemeRegistry::isUserExtensionScheme(StringView scheme)
{
    // FIXME: Remove this once Safari has adopted WKWebViewConfiguration._corsDisablingPatterns
#if PLATFORM(MAC)
    if (scheme == "safari-extension"_s)
        return true;
#else
    UNUSED_PARAM(scheme);
#endif
    return false;
}

bool LegacySchemeRegistry::isBuiltinScheme(StringImpl* scheme)
{
    return !!scheme && (allBuiltinSchemes().contains(scheme) || WTF::URLParser::isSpecialScheme(scheme));
}

} // namespace WebCore
