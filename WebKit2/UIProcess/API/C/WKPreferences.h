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

#ifndef WKPreferences_h
#define WKPreferences_h

#include <WebKit2/WKBase.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKTypeID WKPreferencesGetTypeID();

WK_EXPORT WKPreferencesRef WKPreferencesCreate();
WK_EXPORT WKPreferencesRef WKPreferencesCreateCopy(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetJavaScriptEnabled(WKPreferencesRef preferences, bool javaScriptEnabled);
WK_EXPORT bool WKPreferencesGetJavaScriptEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetLoadsImagesAutomatically(WKPreferencesRef preferences, bool loadsImagesAutomatically);
WK_EXPORT bool WKPreferencesGetLoadsImagesAutomatically(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetOfflineWebApplicationCacheEnabled(WKPreferencesRef preferences, bool offlineWebApplicationCacheEnabled);
WK_EXPORT bool WKPreferencesGetOfflineWebApplicationCacheEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetLocalStorageEnabled(WKPreferencesRef preferences, bool localStorageEnabled);
WK_EXPORT bool WKPreferencesGetLocalStorageEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetXSSAuditorEnabled(WKPreferencesRef preferences, bool xssAuditorEnabled);
WK_EXPORT bool WKPreferencesGetXSSAuditorEnabled(WKPreferencesRef preferences);

WK_EXPORT void WKPreferencesSetStandardFontFamily(WKPreferencesRef preferencesRef, WKStringRef family);
WK_EXPORT WKStringRef WebPreferencesCopyStandardFontFamily(WKPreferencesRef preferencesRef);

WK_EXPORT void WKPreferencesSetFixedFontFamily(WKPreferencesRef preferencesRef, WKStringRef family);
WK_EXPORT WKStringRef WKPreferencesCopyFixedFontFamily(WKPreferencesRef preferencesRef);

WK_EXPORT void WKPreferencesSetSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family);
WK_EXPORT WKStringRef WKPreferencesCopySerifFontFamily(WKPreferencesRef preferencesRef);

WK_EXPORT void WKPreferencesSetSansSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family);
WK_EXPORT WKStringRef WKPreferencesCopySansSerifFontFamily(WKPreferencesRef preferencesRef);

WK_EXPORT void WKPreferencesSetCursiveFontFamily(WKPreferencesRef preferencesRef, WKStringRef family);
WK_EXPORT WKStringRef WKPreferencesCopyCursiveFontFamily(WKPreferencesRef preferencesRef);

WK_EXPORT void WKPreferencesSetFantasyFontFamily(WKPreferencesRef preferencesRef, WKStringRef family);
WK_EXPORT WKStringRef WKPreferencesCopyFantasyFontFamily(WKPreferencesRef preferencesRef);

#ifdef __cplusplus
}
#endif

#endif /* WKPreferences_h */
