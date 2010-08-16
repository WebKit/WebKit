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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

namespace WebCore {

static URLSchemesMap& localURLSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesMap, localSchemes, ());

    if (localSchemes.isEmpty()) {
        localSchemes.add("file");
#if PLATFORM(MAC)
        localSchemes.add("applewebdata");
#endif
#if PLATFORM(QT)
        localSchemes.add("qrc");
#endif
    }

    return localSchemes;
}

static URLSchemesMap& secureSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesMap, secureSchemes, ());

    if (secureSchemes.isEmpty()) {
        secureSchemes.add("https");
        secureSchemes.add("about");
        secureSchemes.add("data");
    }

    return secureSchemes;
}

static URLSchemesMap& schemesWithUniqueOrigins()
{
    DEFINE_STATIC_LOCAL(URLSchemesMap, schemesWithUniqueOrigins, ());

    // This is a willful violation of HTML5.
    // See https://bugs.webkit.org/show_bug.cgi?id=11885
    if (schemesWithUniqueOrigins.isEmpty())
        schemesWithUniqueOrigins.add("data");

    return schemesWithUniqueOrigins;
}

static URLSchemesMap& emptyDocumentSchemes()
{
    DEFINE_STATIC_LOCAL(URLSchemesMap, emptyDocumentSchemes, ());

    if (emptyDocumentSchemes.isEmpty())
        emptyDocumentSchemes.add("about");

    return emptyDocumentSchemes;
}

void SchemeRegistry::registerURLSchemeAsLocal(const String& scheme)
{
    WebCore::localURLSchemes().add(scheme);
}

void SchemeRegistry::removeURLSchemeRegisteredAsLocal(const String& scheme)
{
    if (scheme == "file")
        return;
#if PLATFORM(MAC)
    if (scheme == "applewebdata")
        return;
#endif
    WebCore::localURLSchemes().remove(scheme);
}

const URLSchemesMap& SchemeRegistry::localURLSchemes()
{
    return WebCore::localURLSchemes();
}

bool SchemeRegistry::shouldTreatURLAsLocal(const String& url)
{
    // This avoids an allocation of another String and the HashSet contains()
    // call for the file: and http: schemes.
    if (url.length() >= 5) {
        const UChar* s = url.characters();
        if (s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p' && s[4] == ':')
            return false;
        if (s[0] == 'f' && s[1] == 'i' && s[2] == 'l' && s[3] == 'e' && s[4] == ':')
            return true;
    }

    size_t loc = url.find(':');
    if (loc == notFound)
        return false;

    String scheme = url.left(loc);
    return WebCore::localURLSchemes().contains(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsLocal(const String& scheme)
{
    // This avoids an allocation of another String and the HashSet contains()
    // call for the file: and http: schemes.
    if (scheme.length() == 4) {
        const UChar* s = scheme.characters();
        if (s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p')
            return false;
        if (s[0] == 'f' && s[1] == 'i' && s[2] == 'l' && s[3] == 'e')
            return true;
    }

    if (scheme.isEmpty())
        return false;

    return WebCore::localURLSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsNoAccess(const String& scheme)
{
    schemesWithUniqueOrigins().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsNoAccess(const String& scheme)
{
    return schemesWithUniqueOrigins().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsSecure(const String& scheme)
{
    secureSchemes().add(scheme);
}

bool SchemeRegistry::shouldTreatURLSchemeAsSecure(const String& scheme)
{
    return secureSchemes().contains(scheme);
}

void SchemeRegistry::registerURLSchemeAsEmptyDocument(const String& scheme)
{
    emptyDocumentSchemes().add(scheme);
}

bool SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(const String& scheme)
{
    return emptyDocumentSchemes().contains(scheme);
}

} // namespace WebCore
