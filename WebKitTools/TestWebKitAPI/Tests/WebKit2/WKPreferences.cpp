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

#include "Test.h"

#include <WebKit2/WKPreferences.h>
#include <WebKit2/WKPreferencesPrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKString.h>
#include <wtf/Platform.h>

namespace TestWebKitAPI {

TEST(WebKit2, WKPreferencesBasic)
{
    WKPreferencesRef preference = WKPreferencesCreate();

    TEST_ASSERT(WKGetTypeID(preference) == WKPreferencesGetTypeID());

    WKRelease(preference);
}

TEST(WebKit2, WKPreferencesDefaults)
{
#if PLATFORM(WIN)
    static const char* expectedStandardFontFamily = "Times New Roman";
    static const char* expectedFixedFontFamily = "Courier New";
    static const char* expectedSerifFontFamily = "Times New Roman";
    static const char* expectedSansSerifFontFamily = "Arial";
    static const char* expectedCursiveFontFamily = "Comic Sans MS";
    static const char* expectedFantasyFontFamily = "Comic Sans MS";
#elif PLATFORM(MAC)
    static const char* expectedStandardFontFamily = "Times";
    static const char* expectedFixedFontFamily = "Courier";
    static const char* expectedSerifFontFamily = "Times";
    static const char* expectedSansSerifFontFamily = "Helvetica";
    static const char* expectedCursiveFontFamily = "Apple Chancery";
    static const char* expectedFantasyFontFamily = "Papyrus";
#endif

    WKPreferencesRef preference = WKPreferencesCreate();

    TEST_ASSERT(WKPreferencesGetJavaScriptEnabled(preference) == true);
    TEST_ASSERT(WKPreferencesGetLoadsImagesAutomatically(preference) == true);
    TEST_ASSERT(WKPreferencesGetOfflineWebApplicationCacheEnabled(preference) == false);
    TEST_ASSERT(WKPreferencesGetLocalStorageEnabled(preference) == true);
    TEST_ASSERT(WKPreferencesGetXSSAuditorEnabled(preference) == true);
    TEST_ASSERT(WKPreferencesGetFrameFlatteningEnabled(preference) == false);
    TEST_ASSERT(WKPreferencesGetPluginsEnabled(preference) == true);
    TEST_ASSERT(WKPreferencesGetJavaEnabled(preference) == true);
    TEST_ASSERT(WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(preference) == true);
    TEST_ASSERT(WKPreferencesGetHyperlinkAuditingEnabled(preference) == true);
    WKRetainPtr<WKStringRef> standardFontFamily(AdoptWK, WKPreferencesCopyStandardFontFamily(preference));
    TEST_ASSERT(WKStringIsEqualToUTF8CString(standardFontFamily.get(), expectedStandardFontFamily));
    WKRetainPtr<WKStringRef> fixedFontFamily(AdoptWK, WKPreferencesCopyFixedFontFamily(preference));
    TEST_ASSERT(WKStringIsEqualToUTF8CString(fixedFontFamily.get(), expectedFixedFontFamily));
    WKRetainPtr<WKStringRef> serifFontFamily(AdoptWK, WKPreferencesCopySerifFontFamily(preference));
    TEST_ASSERT(WKStringIsEqualToUTF8CString(serifFontFamily.get(), expectedSerifFontFamily));
    WKRetainPtr<WKStringRef> sansSerifFontFamily(AdoptWK, WKPreferencesCopySansSerifFontFamily(preference));
    TEST_ASSERT(WKStringIsEqualToUTF8CString(sansSerifFontFamily.get(), expectedSansSerifFontFamily));
    WKRetainPtr<WKStringRef> cursiveFontFamily(AdoptWK, WKPreferencesCopyCursiveFontFamily(preference));
    TEST_ASSERT(WKStringIsEqualToUTF8CString(cursiveFontFamily.get(), expectedCursiveFontFamily));
    WKRetainPtr<WKStringRef> fantasyFontFamily(AdoptWK, WKPreferencesCopyFantasyFontFamily(preference));
    TEST_ASSERT(WKStringIsEqualToUTF8CString(fantasyFontFamily.get(), expectedFantasyFontFamily));
    TEST_ASSERT(WKPreferencesGetMinimumFontSize(preference) == 0);
    TEST_ASSERT(WKPreferencesGetPrivateBrowsingEnabled(preference) == false);
    TEST_ASSERT(WKPreferencesGetDeveloperExtrasEnabled(preference) == false);
    TEST_ASSERT(WKPreferencesGetTextAreasAreResizable(preference) == true);
    TEST_ASSERT(WKPreferencesGetFontSmoothingLevel(preference) == kWKFontSmoothingLevelMedium);
    TEST_ASSERT(WKPreferencesGetAcceleratedCompositingEnabled(preference) == true);
    TEST_ASSERT(WKPreferencesGetCompositingBordersVisible(preference) == false);
    TEST_ASSERT(WKPreferencesGetCompositingRepaintCountersVisible(preference) == false);
    TEST_ASSERT(WKPreferencesGetNeedsSiteSpecificQuirks(preference) == false);

    WKRelease(preference);
}

} // namespace TestWebKitAPI
