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
#include "LegacySchemeRegistry.h"

#include "RuntimeApplicationChecks.h"
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

        // Other misc schemes that the LegacySchemeRegistry doesn't know about.
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
    ASSERT(!isInNetworkProcess());
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

void LegacySchemeRegistry::registerURLSchemeAsLocal(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    localURLSchemes().add(scheme);
}

void LegacySchemeRegistry::removeURLSchemeRegisteredAsLocal(const String& scheme)
{
    Locker<Lock> locker(schemeRegistryLock);
    if (builtinLocalURLSchemes().contains(scheme))
        return;

    localURLSchemes().remove(scheme);
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

static URLSchemesMap& alwaysRevalidatedSchemes()
{
    ASSERT(isMainThread());
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return localURLSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    schemesWithUniqueOrigins().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return schemesWithUniqueOrigins().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsDisplayIsolated(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    displayIsolatedURLSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return displayIsolatedURLSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsSecure(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    secureSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return secureSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme)
{
    if (scheme.isNull())
        return;
    emptyDocumentSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme)
{
    return !scheme.isNull() && emptyDocumentSchemes().contains(scheme);
}

void LegacySchemeRegistry::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const String& scheme)
{
    if (scheme.isNull())
        return;

    if (forbidden)
        schemesForbiddenFromDomainRelaxation().add(scheme);
    else
        schemesForbiddenFromDomainRelaxation().remove(scheme);
}

bool LegacySchemeRegistry::isDomainRelaxationForbiddenForURLScheme(const String& scheme)
{
    return !scheme.isNull() && schemesForbiddenFromDomainRelaxation().contains(scheme);
}

bool LegacySchemeRegistry::canDisplayOnlyIfCanRequest(const String& scheme)
{
    ASSERT(!isInNetworkProcess());
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return canDisplayOnlyIfCanRequestSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(const String& scheme)
{
    ASSERT(!isInNetworkProcess());
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    canDisplayOnlyIfCanRequestSchemes().add(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    if (scheme.isNull())
        return;
    notAllowingJavascriptURLsSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    return !scheme.isNull() && notAllowingJavascriptURLsSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(const String& scheme)
{
    if (scheme.isNull())
        return;
    schemesAllowingDatabaseAccessInPrivateBrowsing().add(scheme);
}

bool LegacySchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(const String& scheme)
{
    return !scheme.isNull() && schemesAllowingDatabaseAccessInPrivateBrowsing().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsCORSEnabled(const String& scheme)
{
    ASSERT(!isInNetworkProcess());
    if (scheme.isNull())
        return;
    CORSEnabledSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(const String& scheme)
{
    ASSERT(!isInNetworkProcess());
    return !scheme.isNull() && CORSEnabledSchemes().contains(scheme);
}

Vector<String> LegacySchemeRegistry::allURLSchemesRegisteredAsCORSEnabled()
{
    ASSERT(!isInNetworkProcess());
    return copyToVector(CORSEnabledSchemes());
}

void LegacySchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    ContentSecurityPolicyBypassingSchemes().add(scheme);
}

void LegacySchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    ContentSecurityPolicyBypassingSchemes().remove(scheme);
}

bool LegacySchemeRegistry::schemeShouldBypassContentSecurityPolicy(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return ContentSecurityPolicyBypassingSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsAlwaysRevalidated(const String& scheme)
{
    if (scheme.isNull())
        return;
    alwaysRevalidatedSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldAlwaysRevalidateURLScheme(const String& scheme)
{
    return !scheme.isNull() && alwaysRevalidatedSchemes().contains(scheme);
}

void LegacySchemeRegistry::registerURLSchemeAsCachePartitioned(const String& scheme)
{
    if (scheme.isNull())
        return;

    Locker<Lock> locker(schemeRegistryLock);
    cachePartitioningSchemes().add(scheme);
}

bool LegacySchemeRegistry::shouldPartitionCacheForURLScheme(const String& scheme)
{
    if (scheme.isNull())
        return false;

    Locker<Lock> locker(schemeRegistryLock);
    return cachePartitioningSchemes().contains(scheme);
}

bool LegacySchemeRegistry::isUserExtensionScheme(const String& scheme)
{
    // FIXME: Remove this once Safari has adopted WKWebViewConfiguration._corsDisablingPatterns
#if PLATFORM(MAC)
    if (scheme == "safari-extension")
        return true;
#else
    UNUSED_PARAM(scheme);
#endif
    return false;
}

bool LegacySchemeRegistry::isBuiltinScheme(const String& scheme)
{
    return !scheme.isNull() && (allBuiltinSchemes().contains(scheme) || WTF::URLParser::isSpecialScheme(scheme));
}

} // namespace WebCore
