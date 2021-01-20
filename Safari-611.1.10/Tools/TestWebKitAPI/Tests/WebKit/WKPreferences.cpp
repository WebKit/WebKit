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

#include "config.h"

#if WK_HAVE_C_SPI

#include "PlatformUtilities.h"
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit, WKPreferencesBasic)
{
    WKPreferencesRef preference = WKPreferencesCreate();

    EXPECT_EQ(WKPreferencesGetTypeID(), WKGetTypeID(preference));

    WKRelease(preference);
}

TEST(WebKit, WKPreferencesDefaults)
{
#if PLATFORM(GTK) || PLATFORM(PLAYSTATION)
    static const char* expectedStandardFontFamily = "Times";
    static const char* expectedFixedFontFamily = "Courier New";
    static const char* expectedSerifFontFamily = "Times";
    static const char* expectedSansSerifFontFamily = "Helvetica";
    static const char* expectedCursiveFontFamily = "Comic Sans MS";
    static const char* expectedFantasyFontFamily = "Impact";
    static const char* expectedPictographFontFamily = "Times";
#elif WK_HAVE_C_SPI
    static const char* expectedStandardFontFamily = "Times";
    static const char* expectedFixedFontFamily = "Courier";
    static const char* expectedSerifFontFamily = "Times";
    static const char* expectedSansSerifFontFamily = "Helvetica";
    static const char* expectedCursiveFontFamily = "Apple Chancery";
    static const char* expectedFantasyFontFamily = "Papyrus";
    static const char* expectedPictographFontFamily = "Apple Color Emoji";
#elif PLATFORM(IOS_FAMILY)
    static const char* expectedStandardFontFamily = "Times";
    static const char* expectedFixedFontFamily = "Courier";
    static const char* expectedSerifFontFamily = "Times";
    static const char* expectedSansSerifFontFamily = "Helvetica";
    static const char* expectedCursiveFontFamily = "Snell Roundhand";
    static const char* expectedFantasyFontFamily = "Papyrus";
    static const char* expectedPictographFontFamily = "AppleColorEmoji";
#endif

    WKPreferencesRef preference = WKPreferencesCreate();

    EXPECT_TRUE(WKPreferencesGetJavaScriptEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetLoadsImagesAutomatically(preference));
    EXPECT_TRUE(WKPreferencesGetOfflineWebApplicationCacheEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetLocalStorageEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetXSSAuditorEnabled(preference));
    EXPECT_FALSE(WKPreferencesGetFrameFlatteningEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetPluginsEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetJavaEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(preference));
    EXPECT_TRUE(WKPreferencesGetHyperlinkAuditingEnabled(preference));
    EXPECT_WK_STREQ(expectedStandardFontFamily, adoptWK(WKPreferencesCopyStandardFontFamily(preference)));
    EXPECT_WK_STREQ(expectedFixedFontFamily, adoptWK(WKPreferencesCopyFixedFontFamily(preference)));
    EXPECT_WK_STREQ(expectedSerifFontFamily, adoptWK(WKPreferencesCopySerifFontFamily(preference)));
    EXPECT_WK_STREQ(expectedSansSerifFontFamily, adoptWK(WKPreferencesCopySansSerifFontFamily(preference)));
    EXPECT_WK_STREQ(expectedCursiveFontFamily, adoptWK(WKPreferencesCopyCursiveFontFamily(preference)));
    EXPECT_WK_STREQ(expectedFantasyFontFamily, adoptWK(WKPreferencesCopyFantasyFontFamily(preference)));
    EXPECT_WK_STREQ(expectedPictographFontFamily, adoptWK(WKPreferencesCopyPictographFontFamily(preference)));
    EXPECT_EQ(0u, WKPreferencesGetMinimumFontSize(preference));
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    EXPECT_FALSE(WKPreferencesGetPrivateBrowsingEnabled(preference));
    ALLOW_DEPRECATED_DECLARATIONS_END
    EXPECT_FALSE(WKPreferencesGetDeveloperExtrasEnabled(preference));
    EXPECT_TRUE(WKPreferencesGetTextAreasAreResizable(preference));

    EXPECT_TRUE(WKPreferencesGetAcceleratedCompositingEnabled(preference));
    EXPECT_FALSE(WKPreferencesGetCompositingBordersVisible(preference));
    EXPECT_FALSE(WKPreferencesGetCompositingRepaintCountersVisible(preference));
    EXPECT_TRUE(WKPreferencesGetNeedsSiteSpecificQuirks(preference));
    EXPECT_EQ(kWKAllowAllStorage, WKPreferencesGetStorageBlockingPolicy(preference));
    EXPECT_FALSE(WKPreferencesGetTextAutosizingEnabled(preference));

    WKRelease(preference);
}

TEST(WebKit, WKPreferencesCopying)
{
    WKRetainPtr<WKStringRef> identifier = adoptWK(WKStringCreateWithUTF8CString("identifier"));

    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreateWithIdentifier(identifier.get()));
    WKPreferencesSetDefaultFontSize(preferences.get(), 36);

    WKRetainPtr<WKPreferencesRef> copy = adoptWK(WKPreferencesCreateCopy(preferences.get()));

    WKPreferencesSetDefaultFontSize(preferences.get(), 24);
    EXPECT_EQ(24u, WKPreferencesGetDefaultFontSize(preferences.get()));
    EXPECT_EQ(36u, WKPreferencesGetDefaultFontSize(copy.get()));
}

} // namespace TestWebKitAPI

#endif
