/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
#include "SchemeRegistry.h"

#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
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
static const Vector<String>& builtinSecureSchemes();
static const Vector<String>& builtinSchemesWithUniqueOrigins();
static const Vector<String>& builtinEmptyDocumentSchemes();
static const Vector<String>& builtinCanDisplayOnlyIfCanRequestSchemes();
static const Vector<String>& builtinCORSEnabledSchemes();

using StringVectorFunction = const Vector<String>& (*)();

static void add(URLSchemesMap& set, StringVectorFunction function)
{
    for (auto& scheme : function())
        set.add(scheme);
}

static NeverDestroyed<URLSchemesMap> makeNeverDestroyedSchemeSet(const Vector<String>& (*function)())
{
    URLSchemesMap set;
    add(set, function);
    return set;
}

static Lock schemeRegistryLock;

static const URLSchemesMap& allBuiltinSchemes()
{
    static const auto schemes = makeNeverDestroyed([] {
        static const StringVectorFunction functions[] {
            builtinSecureSchemes,
            builtinSchemesWithUniqueOrigins,
            builtinEmptyDocumentSchemes,
            builtinCanDisplayOnlyIfCanRequestSchemes,
            builtinCORSEnabledSchemes,
        };

        // Other misc schemes that the SchemeRegistry doesn't know about.
        static const char* const otherSchemes[] = {
            "webkit-fake-url",
#if PLATFORM(MAC)
            "safari-extension",
#endif
#if USE(QUICK_LOOK)
            QLPreviewProtocol,
#endif
#if ENABLE(CONTENT_FILTERING)
            ContentFilter::urlScheme(),
#endif
        };

        URLSchemesMap set;
        {
            Locker<Lock> locker(schemeRegistryLock);
            for (auto& scheme : builtinLocalURLSchemes())
                set.add(scheme);

            for (auto& function : functions)
                add(set, function);
        }
        for (auto& scheme : otherSchemes)
            set.add(scheme);
        return set;
    }());
    return schemes;
}

static const URLSchemesMap& builtinLocalURLSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static const auto schemes = makeNeverDestroyed(URLSchemesMap {
        "file",
#if PLATFORM(COCOA)
        "applewebdata",
#endif
    });
    return schemes;
}

static URLSchemesMap& localURLSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> localSchemes = builtinLocalURLSchemes();
    return localSchemes;
}

static URLSchemesMap& displayIsolatedURLSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> displayIsolatedSchemes;
    return displayIsolatedSchemes;
}

const Vector<String>& builtinSecureSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static const auto schemes = makeNeverDestroyed(Vector<String> {
        "https",
        "about",
        "data",
        "wss",
#if PLATFORM(GTK) || PLATFORM(WPE)
        "resource",
#endif
    });
    return schemes;
}

static URLSchemesMap& secureSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static auto secureSchemes = makeNeverDestroyedSchemeSet(builtinSecureSchemes);
    return secureSchemes;
}

const Vector<String>& builtinSchemesWithUniqueOrigins()
{
    ASSERT(schemeRegistryLock.isHeld());
    static const auto schemes = makeNeverDestroyed(Vector<String> {
        "about",
        "javascript",
        // This is an intentional difference from the behavior the HTML specification calls for.
        // See https://bugs.webkit.org/show_bug.cgi?id=11885
        "data",
    });
    return schemes;
}

static URLSchemesMap& schemesWithUniqueOrigins()
{
    ASSERT(schemeRegistryLock.isHeld());
    static auto schemesWithUniqueOrigins = makeNeverDestroyedSchemeSet(builtinSchemesWithUniqueOrigins);
    return schemesWithUniqueOrigins;
}

const Vector<String>& builtinEmptyDocumentSchemes()
{
    ASSERT(isMainThread());
    static const auto schemes = makeNeverDestroyed(Vector<String> { "about" });
    return schemes;
}

static URLSchemesMap& emptyDocumentSchemes()
{
    ASSERT(isMainThread());
    static auto emptyDocumentSchemes = makeNeverDestroyedSchemeSet(builtinEmptyDocumentSchemes);
    return emptyDocumentSchemes;
}

static URLSchemesMap& schemesForbiddenFromDomainRelaxation()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

const Vector<String>& builtinCanDisplayOnlyIfCanRequestSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static const auto schemes = makeNeverDestroyed(Vector<String> { "blob" });
    return schemes;
}

static URLSchemesMap& canDisplayOnlyIfCanRequestSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static auto canDisplayOnlyIfCanRequestSchemes = makeNeverDestroyedSchemeSet(builtinCanDisplayOnlyIfCanRequestSchemes);
    return canDisplayOnlyIfCanRequestSchemes;
}

static URLSchemesMap& notAllowingJavascriptURLsSchemes()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> notAllowingJavascriptURLsSchemes;
    return notAllowingJavascriptURLsSchemes;
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    localURLSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeRegisteredAsLocal(const String& scheme)
{
    Locker<Lock> locker(schemeRegistryLock);
    if (builtinLocalURLSchemes().contains(scheme))
        return;

    localURLSchemes().remove(scheme);
}

static URLSchemesMap& schemesAllowingLocalStorageAccessInPrivateBrowsing()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemesAllowingLocalStorageAccessInPrivateBrowsing;
    return schemesAllowingLocalStorageAccessInPrivateBrowsing;
}

static URLSchemesMap& schemesAllowingDatabaseAccessInPrivateBrowsing()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemesAllowingDatabaseAccessInPrivateBrowsing;
    return schemesAllowingDatabaseAccessInPrivateBrowsing;
}

const Vector<String>& builtinCORSEnabledSchemes()
{
    ASSERT(isMainThread());
    static const auto schemes = makeNeverDestroyed(Vector<String> { "http", "https" });
    return schemes;
}

static URLSchemesMap& CORSEnabledSchemes()
{
    ASSERT(isMainThread());
    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=77160
    static auto schemes = makeNeverDestroyedSchemeSet(builtinCORSEnabledSchemes);
    return schemes;
}

static URLSchemesMap& ContentSecurityPolicyBypassingSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

static URLSchemesMap& cachePartitioningSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

static URLSchemesMap& serviceWorkerSchemes()
{
    ASSERT(schemeRegistryLock.isHeld());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

static URLSchemesMap& alwaysRevalidatedSchemes()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

bool SchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return localURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    schemesWithUniqueOrigins().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return schemesWithUniqueOrigins().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsDisplayIsolated(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    displayIsolatedURLSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return displayIsolatedURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsSecure(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    secureSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return secureSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme)
{
    if (scheme.isNull())
        return;
    emptyDocumentSchemes().add(scheme);
}

bool SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme)
{
    return !scheme.isNull() && emptyDocumentSchemes().contains(scheme);
}

void SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const String& scheme)
{
    if (scheme.isNull())
        return;

    if (forbidden)
        schemesForbiddenFromDomainRelaxation().add(scheme);
    else
        schemesForbiddenFromDomainRelaxation().remove(scheme);
}

bool SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(const String& scheme)
{
    return !scheme.isNull() && schemesForbiddenFromDomainRelaxation().contains(scheme);
}

bool SchemeRegistry::canDisplayOnlyIfCanRequest(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return canDisplayOnlyIfCanRequestSchemes().contains(scheme);
}

void SchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    canDisplayOnlyIfCanRequestSchemes().add(scheme);
}

void SchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    if (scheme.isNull())
        return;
    notAllowingJavascriptURLsSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    return !scheme.isNull() && notAllowingJavascriptURLsSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAllowingLocalStorageAccessInPrivateBrowsing(const String& scheme)
{
    if (scheme.isNull())
        return;
    schemesAllowingLocalStorageAccessInPrivateBrowsing().add(scheme);
}

bool SchemeRegistry::allowsLocalStorageAccessInPrivateBrowsing(const String& scheme)
{
    return !scheme.isNull() && schemesAllowingLocalStorageAccessInPrivateBrowsing().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(const String& scheme)
{
    if (scheme.isNull())
        return;
    schemesAllowingDatabaseAccessInPrivateBrowsing().add(scheme);
}

bool SchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(const String& scheme)
{
    return !scheme.isNull() && schemesAllowingDatabaseAccessInPrivateBrowsing().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsCORSEnabled(const String& scheme)
{
    if (scheme.isNull())
        return;
    CORSEnabledSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(const String& scheme)
{
    return !scheme.isNull() && CORSEnabledSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    ContentSecurityPolicyBypassingSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    ContentSecurityPolicyBypassingSchemes().remove(scheme);
}

bool SchemeRegistry::schemeShouldBypassContentSecurityPolicy(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return ContentSecurityPolicyBypassingSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAlwaysRevalidated(const String& scheme)
{
    if (scheme.isNull())
        return;
    alwaysRevalidatedSchemes().add(scheme);
}

bool SchemeRegistry::shouldAlwaysRevalidateURLScheme(const String& scheme)
{
    return !scheme.isNull() && alwaysRevalidatedSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsCachePartitioned(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    cachePartitioningSchemes().add(scheme);
}

bool SchemeRegistry::shouldPartitionCacheForURLScheme(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return cachePartitioningSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeServiceWorkersCanHandle(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    serviceWorkerSchemes().add(scheme);
}

bool SchemeRegistry::canServiceWorkersHandleURLScheme(const String& scheme)
{
    if (scheme.isNull())
        return false;

    if (scheme.startsWithIgnoringASCIICase("http"_s)) {
        if (scheme.length() == 4)
            return true;
        if (scheme.length() == 5 && isASCIIAlphaCaselessEqual(scheme[4], 's'))
            return true;
    }

    Locker<Lock> locker(schemeRegistryLock);
    return serviceWorkerSchemes().contains(scheme);
}

bool SchemeRegistry::isServiceWorkerContainerCustomScheme(const String& scheme)
{
    Locker<Lock> locker(schemeRegistryLock);
    return !scheme.isNull() && serviceWorkerSchemes().contains(scheme);
}

bool SchemeRegistry::isUserExtensionScheme(const String& scheme)
{
#if PLATFORM(MAC)
    if (scheme == "safari-extension")
        return true;
#else
    UNUSED_PARAM(scheme);
#endif
    return false;
}

bool SchemeRegistry::isBuiltinScheme(const String& scheme)
{
    return !scheme.isNull() && (allBuiltinSchemes().contains(scheme) || WTF::URLParser::isSpecialScheme(scheme));
}

} // namespace WebCore
