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

#pragma once

#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// FIXME: Make HashSet<String>::contains(StringView) work and use StringViews here.
using URLSchemesMap = HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>;

class LegacySchemeRegistry {
public:
    WEBCORE_EXPORT static void registerURLSchemeAsLocal(StringImpl*); // Thread safe.
    static void removeURLSchemeRegisteredAsLocal(StringImpl*); // Thread safe.

    WEBCORE_EXPORT static bool shouldTreatURLSchemeAsLocal(StringView); // Thread safe.
    WEBCORE_EXPORT static bool isBuiltinScheme(StringImpl*);
    
    // Secure schemes do not trigger mixed content warnings. For example,
    // https and data are secure schemes because they cannot be corrupted by
    // active network attackers.
    WEBCORE_EXPORT static void registerURLSchemeAsSecure(StringImpl*); // Thread safe.
    static bool shouldTreatURLSchemeAsSecure(StringView); // Thread safe.

    WEBCORE_EXPORT static void registerURLSchemeAsNoAccess(StringImpl*); // Thread safe.
    static bool shouldTreatURLSchemeAsNoAccess(StringView); // Thread safe.

    WEBCORE_EXPORT static void registerURLSchemeAsAllowingServiceWorkerClients(StringImpl*); // Must be called on the main thread.
    static bool shouldTreatURLSchemeAsAllowingServiceWorkerClients(StringView); // Must be called on the main thread.

    // Display-isolated schemes can only be displayed (in the sense of
    // SecurityOrigin::canDisplay) by documents from the same scheme.
    WEBCORE_EXPORT static void registerURLSchemeAsDisplayIsolated(StringImpl*); // Thread safe.
    static bool shouldTreatURLSchemeAsDisplayIsolated(StringView); // Thread safe.

    WEBCORE_EXPORT static void registerURLSchemeAsEmptyDocument(StringImpl*);
    WEBCORE_EXPORT static bool shouldLoadURLSchemeAsEmptyDocument(StringView);

    WEBCORE_EXPORT static void setDomainRelaxationForbiddenForURLScheme(bool forbidden, StringImpl*);
    static bool isDomainRelaxationForbiddenForURLScheme(StringImpl*);

    // Such schemes should delegate to SecurityOrigin::canRequest for any URL
    // passed to SecurityOrigin::canDisplay.
    static bool canDisplayOnlyIfCanRequest(StringView scheme); // Thread safe.
    WEBCORE_EXPORT static void registerAsCanDisplayOnlyIfCanRequest(StringImpl* scheme); // Thread safe.

    // Schemes against which javascript: URLs should not be allowed to run (stop
    // bookmarklets from running on sensitive pages). 
    static void registerURLSchemeAsNotAllowingJavascriptURLs(StringImpl* scheme);
    static bool shouldTreatURLSchemeAsNotAllowingJavascriptURLs(StringImpl* scheme);

    // Let some schemes opt-out of Private Browsing's default behavior of prohibiting read/write
    // access to Local Storage and Databases.
    WEBCORE_EXPORT static void registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(StringImpl* scheme);
    static bool allowsDatabaseAccessInPrivateBrowsing(StringImpl* scheme);

    // Allow non-HTTP schemes to be registered to allow CORS requests.
    WEBCORE_EXPORT static void registerURLSchemeAsCORSEnabled(StringImpl* scheme);
    WEBCORE_EXPORT static bool shouldTreatURLSchemeAsCORSEnabled(StringView scheme);
    WEBCORE_EXPORT static Vector<RefPtr<StringImpl>> allURLSchemesRegisteredAsCORSEnabled();

    WEBCORE_EXPORT static void registerURLSchemeAsHandledBySchemeHandler(StringImpl*);
    WEBCORE_EXPORT static bool schemeIsHandledBySchemeHandler(StringView);

    // Allow resources from some schemes to load on a page, regardless of its
    // Content Security Policy.
    WEBCORE_EXPORT static void registerURLSchemeAsBypassingContentSecurityPolicy(StringImpl* scheme); // Thread safe.
    WEBCORE_EXPORT static void removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(StringImpl* scheme); // Thread safe.
    static bool schemeShouldBypassContentSecurityPolicy(StringView scheme); // Thread safe.

    // Schemes whose responses should always be revalidated.
    WEBCORE_EXPORT static void registerURLSchemeAsAlwaysRevalidated(StringImpl*);
    static bool shouldAlwaysRevalidateURLScheme(StringView);

    // Schemes whose requests should be partitioned in the cache
    WEBCORE_EXPORT static void registerURLSchemeAsCachePartitioned(StringImpl* scheme); // Thread safe.
    static bool shouldPartitionCacheForURLScheme(StringImpl* scheme); // Thread safe.

    static bool isUserExtensionScheme(StringView scheme);
};

} // namespace WebCore
