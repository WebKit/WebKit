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

#include "URLParser.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(CONTENT_FILTERING)
#include "ContentFilter.h"
#endif
#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

namespace WebCore {

static const URLSchemesMap& builtinLocalURLSchemes();
static const Vector<String>& builtinSecureSchemes();
static const Vector<String>& builtinSchemesWithUniqueOrigins();
static const Vector<String>& builtinEmptyDocumentSchemes();
static const Vector<String>& builtinCanDisplayOnlyIfCanRequestSchemes();
static const Vector<String>& builtinCORSEnabledSchemes();

static const URLSchemesMap& allBuiltinSchemes()
{
    static NeverDestroyed<URLSchemesMap> schemes;
    if (schemes.get().isEmpty()) {
        for (const auto& scheme : builtinLocalURLSchemes())
            schemes.get().add(scheme);
        for (const auto& scheme : builtinSecureSchemes())
            schemes.get().add(scheme);
        for (const auto& scheme : builtinSchemesWithUniqueOrigins())
            schemes.get().add(scheme);
        for (const auto& scheme : builtinEmptyDocumentSchemes())
            schemes.get().add(scheme);
        for (const auto& scheme : builtinCanDisplayOnlyIfCanRequestSchemes())
            schemes.get().add(scheme);
        for (const auto& scheme : builtinCORSEnabledSchemes())
            schemes.get().add(scheme);

        // Other misc schemes that the SchemeRegistry doesn't know about.
        schemes.get().add("webkit-fake-url");
#if PLATFORM(MAC)
        schemes.get().add("safari-extension");
#endif
#if USE(QUICK_LOOK)
        schemes.get().add(QLPreviewProtocol());
#endif
#if ENABLE(CONTENT_FILTERING)
        schemes.get().add(ContentFilter::urlScheme());
#endif
    }

    return schemes;
}

const URLSchemesMap& builtinLocalURLSchemes()
{
    static NeverDestroyed<URLSchemesMap> schemes;
    if (schemes.get().isEmpty()) {
        schemes.get().add("file");
#if PLATFORM(COCOA)
        schemes.get().add("applewebdata");
#endif
    }

    return schemes;
}

static URLSchemesMap& localURLSchemes()
{
    static NeverDestroyed<URLSchemesMap> localSchemes;

    if (localSchemes.get().isEmpty()) {
        for (const auto& scheme : builtinLocalURLSchemes())
            localSchemes.get().add(scheme);
    }

    return localSchemes;
}

static URLSchemesMap& displayIsolatedURLSchemes()
{
    static NeverDestroyed<URLSchemesMap> displayIsolatedSchemes;
    return displayIsolatedSchemes;
}

const Vector<String>& builtinSecureSchemes()
{
    static NeverDestroyed<Vector<String>> schemes;
    if (schemes.get().isEmpty()) {
        schemes.get().append("https");
        schemes.get().append("about");
        schemes.get().append("data");
        schemes.get().append("wss");
#if PLATFORM(GTK) || PLATFORM(WPE)
        schemes.get().append("resource");
#endif
        schemes.get().shrinkToFit();
    }

    return schemes;
}

static URLSchemesMap& secureSchemes()
{
    static NeverDestroyed<URLSchemesMap> secureSchemes;

    if (secureSchemes.get().isEmpty()) {
        for (const auto& scheme : builtinSecureSchemes())
            secureSchemes.get().add(scheme);
    }

    return secureSchemes;
}

const Vector<String>& builtinSchemesWithUniqueOrigins()
{
    static NeverDestroyed<Vector<String>> schemes;
    if (schemes.get().isEmpty()) {
        schemes.get().append("about");
        schemes.get().append("javascript");
        // This is a willful violation of HTML5.
        // See https://bugs.webkit.org/show_bug.cgi?id=11885
        schemes.get().append("data");
        schemes.get().shrinkToFit();
    }

    return schemes;
}

static URLSchemesMap& schemesWithUniqueOrigins()
{
    static NeverDestroyed<URLSchemesMap> schemesWithUniqueOrigins;

    if (schemesWithUniqueOrigins.get().isEmpty()) {
        for (const auto& scheme : builtinSchemesWithUniqueOrigins())
            schemesWithUniqueOrigins.get().add(scheme);
    }

    return schemesWithUniqueOrigins;
}

const Vector<String>& builtinEmptyDocumentSchemes()
{
    static NeverDestroyed<Vector<String>> schemes;
    if (schemes.get().isEmpty()) {
        schemes.get().append("about");
        schemes.get().shrinkToFit();
    }

    return schemes;
}

static URLSchemesMap& emptyDocumentSchemes()
{
    static NeverDestroyed<URLSchemesMap> emptyDocumentSchemes;

    if (emptyDocumentSchemes.get().isEmpty()) {
        for (const auto& scheme : builtinEmptyDocumentSchemes())
            emptyDocumentSchemes.get().add(scheme);
    }

    return emptyDocumentSchemes;
}

static HashSet<String, ASCIICaseInsensitiveHash>& schemesForbiddenFromDomainRelaxation()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> schemes;
    return schemes;
}

const Vector<String>& builtinCanDisplayOnlyIfCanRequestSchemes()
{
    static NeverDestroyed<Vector<String>> schemes;
    if (schemes.get().isEmpty()) {
        schemes.get().append("blob");
        schemes.get().shrinkToFit();
    }

    return schemes;
}

static URLSchemesMap& canDisplayOnlyIfCanRequestSchemes()
{
    static NeverDestroyed<URLSchemesMap> canDisplayOnlyIfCanRequestSchemes;

    if (canDisplayOnlyIfCanRequestSchemes.get().isEmpty()) {
        for (const auto& scheme : builtinCanDisplayOnlyIfCanRequestSchemes())
            canDisplayOnlyIfCanRequestSchemes.get().add(scheme);
    }

    return canDisplayOnlyIfCanRequestSchemes;
}

static URLSchemesMap& notAllowingJavascriptURLsSchemes()
{
    static NeverDestroyed<URLSchemesMap> notAllowingJavascriptURLsSchemes;
    return notAllowingJavascriptURLsSchemes;
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme)
{
    localURLSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeRegisteredAsLocal(const String& scheme)
{
    if (builtinLocalURLSchemes().contains(scheme))
        return;

    localURLSchemes().remove(scheme);
}

const URLSchemesMap& SchemeRegistry::localSchemes()
{
    return localURLSchemes();
}

static URLSchemesMap& schemesAllowingLocalStorageAccessInPrivateBrowsing()
{
    static NeverDestroyed<URLSchemesMap> schemesAllowingLocalStorageAccessInPrivateBrowsing;
    return schemesAllowingLocalStorageAccessInPrivateBrowsing;
}

static URLSchemesMap& schemesAllowingDatabaseAccessInPrivateBrowsing()
{
    static NeverDestroyed<URLSchemesMap> schemesAllowingDatabaseAccessInPrivateBrowsing;
    return schemesAllowingDatabaseAccessInPrivateBrowsing;
}

const Vector<String>& builtinCORSEnabledSchemes()
{
    static NeverDestroyed<Vector<String>> schemes;
    if (schemes.get().isEmpty()) {
        schemes.get().append("http");
        schemes.get().append("https");
        schemes.get().shrinkToFit();
    }

    return schemes;
}

static URLSchemesMap& CORSEnabledSchemes()
{
    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=77160
    static NeverDestroyed<URLSchemesMap> CORSEnabledSchemes;

    if (CORSEnabledSchemes.get().isEmpty()) {
        for (const auto& scheme : builtinCORSEnabledSchemes())
            CORSEnabledSchemes.get().add(scheme);
    }

    return CORSEnabledSchemes;
}

static URLSchemesMap& ContentSecurityPolicyBypassingSchemes()
{
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

static URLSchemesMap& cachePartitioningSchemes()
{
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

static URLSchemesMap& alwaysRevalidatedSchemes()
{
    static NeverDestroyed<URLSchemesMap> schemes;
    return schemes;
}

bool SchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return localURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme)
{
    schemesWithUniqueOrigins().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return schemesWithUniqueOrigins().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsDisplayIsolated(const String& scheme)
{
    displayIsolatedURLSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return displayIsolatedURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsSecure(const String& scheme)
{
    secureSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return secureSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme)
{
    emptyDocumentSchemes().add(scheme);
}

bool SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return emptyDocumentSchemes().contains(scheme);
}

void SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const String& scheme)
{
    if (scheme.isEmpty())
        return;

    if (forbidden)
        schemesForbiddenFromDomainRelaxation().add(scheme);
    else
        schemesForbiddenFromDomainRelaxation().remove(scheme);
}

bool SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return schemesForbiddenFromDomainRelaxation().contains(scheme);
}

bool SchemeRegistry::canDisplayOnlyIfCanRequest(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return canDisplayOnlyIfCanRequestSchemes().contains(scheme);
}

void SchemeRegistry::registerAsCanDisplayOnlyIfCanRequest(const String& scheme)
{
    canDisplayOnlyIfCanRequestSchemes().add(scheme);
}

void SchemeRegistry::registerURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    notAllowingJavascriptURLsSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return notAllowingJavascriptURLsSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAllowingLocalStorageAccessInPrivateBrowsing(const String& scheme)
{
    schemesAllowingLocalStorageAccessInPrivateBrowsing().add(scheme);
}

bool SchemeRegistry::allowsLocalStorageAccessInPrivateBrowsing(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return schemesAllowingLocalStorageAccessInPrivateBrowsing().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(const String& scheme)
{
    schemesAllowingDatabaseAccessInPrivateBrowsing().add(scheme);
}

bool SchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return schemesAllowingDatabaseAccessInPrivateBrowsing().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsCORSEnabled(const String& scheme)
{
    CORSEnabledSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return CORSEnabledSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    ContentSecurityPolicyBypassingSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    ContentSecurityPolicyBypassingSchemes().remove(scheme);
}

bool SchemeRegistry::schemeShouldBypassContentSecurityPolicy(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return ContentSecurityPolicyBypassingSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsAlwaysRevalidated(const String& scheme)
{
    alwaysRevalidatedSchemes().add(scheme);
}

bool SchemeRegistry::shouldAlwaysRevalidateURLScheme(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return alwaysRevalidatedSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsCachePartitioned(const String& scheme)
{
    cachePartitioningSchemes().add(scheme);
}

bool SchemeRegistry::shouldPartitionCacheForURLScheme(const String& scheme)
{
    if (scheme.isEmpty())
        return false;
    return cachePartitioningSchemes().contains(scheme);
}

bool SchemeRegistry::isUserExtensionScheme(const String& scheme)
{
    UNUSED_PARAM(scheme);
#if PLATFORM(MAC)
    if (scheme == "safari-extension")
        return true;
#endif
    return false;
}

bool SchemeRegistry::isBuiltinScheme(const String& scheme)
{
    return allBuiltinSchemes().contains(scheme) || URLParser::isSpecialScheme(scheme);
}

} // namespace WebCore
