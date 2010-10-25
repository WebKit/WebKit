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

#include "WKPreferences.h"

#include "WKAPICast.h"
#include "WebPreferences.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

using namespace WebKit;

WKTypeID WKPreferencesGetTypeID()
{
    return toAPI(WebPreferences::APIType);
}

WKPreferencesRef WKPreferencesCreate()
{
    RefPtr<WebPreferences> preferences = WebPreferences::create();
    return toAPI(preferences.release().leakRef());
}

WKPreferencesRef WKPreferencesCreateCopy(WKPreferencesRef preferencesRef)
{
    RefPtr<WebPreferences> preferences = WebPreferences::copy(toImpl(preferencesRef));
    return toAPI(preferences.release().releaseRef());
}

void WKPreferencesSetJavaScriptEnabled(WKPreferencesRef preferencesRef, bool javaScriptEnabled)
{
    toImpl(preferencesRef)->setJavaScriptEnabled(javaScriptEnabled);
}

bool WKPreferencesGetJavaScriptEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptEnabled();
}

void WKPreferencesSetLoadsImagesAutomatically(WKPreferencesRef preferencesRef, bool loadsImagesAutomatically)
{
    toImpl(preferencesRef)->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

bool WKPreferencesGetLoadsImagesAutomatically(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->loadsImagesAutomatically();
}

void WKPreferencesSetOfflineWebApplicationCacheEnabled(WKPreferencesRef preferencesRef, bool offlineWebApplicationCacheEnabled)
{
    toImpl(preferencesRef)->setOfflineWebApplicationCacheEnabled(offlineWebApplicationCacheEnabled);
}

bool WKPreferencesGetOfflineWebApplicationCacheEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->offlineWebApplicationCacheEnabled();
}

void WKPreferencesSetLocalStorageEnabled(WKPreferencesRef preferencesRef, bool localStorageEnabled)
{
    toImpl(preferencesRef)->setLocalStorageEnabled(localStorageEnabled);
}

bool WKPreferencesGetLocalStorageEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->localStorageEnabled();
}

void WKPreferencesSetXSSAuditorEnabled(WKPreferencesRef preferencesRef, bool xssAuditorEnabled)
{
    toImpl(preferencesRef)->setXSSAuditorEnabled(xssAuditorEnabled);
}

bool WKPreferencesGetXSSAuditorEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->xssAuditorEnabled();
}

void WKPreferencesSetFrameFlatteningEnabled(WKPreferencesRef preferencesRef, bool frameFlatteningEnabled)
{
    toImpl(preferencesRef)->setFrameFlatteningEnabled(frameFlatteningEnabled);
}

bool WKPreferencesGetFrameFlatteningEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->frameFlatteningEnabled();
}

void WKPreferencesSetPluginsEnabled(WKPreferencesRef preferencesRef, bool pluginsEnabled)
{
    toImpl(preferencesRef)->setPluginsEnabled(pluginsEnabled);
}

bool WKPreferencesGetPluginsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->pluginsEnabled();
}

void WKPReferencesSetJavaEnabled(WKPreferencesRef preferencesRef, bool javaEnabled)
{
    toImpl(preferencesRef)->setJavaEnabled(javaEnabled);
}

bool WKPReferencesGetJavaEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaEnabled();
}

void WKPreferencesSetStandardFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setStandardFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyStandardFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->standardFontFamily());
}

void WKPreferencesSetFixedFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setFixedFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFixedFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->fixedFontFamily());
}

void WKPreferencesSetSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->serifFontFamily());
}

void WKPreferencesSetSansSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setSansSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySansSerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->sansSerifFontFamily());
}

void WKPreferencesSetCursiveFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setCursiveFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyCursiveFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->cursiveFontFamily());
}

void WKPreferencesSetFantasyFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setFantasyFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFantasyFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->fantasyFontFamily());
}

void WKPreferencesSetPrivateBrowsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPrivateBrowsingEnabled(enabled);
}

bool WKPreferencesGetPrivateBrowsingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->privateBrowsingEnabled();
}
