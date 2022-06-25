/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebPreferences.h>

WTF_EXTERN_C_BEGIN
WebCacheModel TestWebPreferencesCacheModelForMainBundle(NSString *bundleIdentifier);
WTF_EXTERN_C_END

namespace TestWebKitAPI {

TEST(WebKitLegacy, CacheModelForMainBundle)
{
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"Microsoft/com.microsoft.Messenger"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.adiumX.adiumX"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.alientechnology.Proteus"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.apple.Dashcode"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.apple.iChat"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.barebones.bbedit"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.barebones.textwrangler"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.barebones.yojimbo"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.equinux.iSale4"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.growl.growlframework"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.intrarts.PandoraMan"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.karelia.Sandvox"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.macromates.textmate"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.realmacsoftware.rapidweaverpro"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.red-sweater.marsedit"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.yahoo.messenger3"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"de.codingmonkeys.SubEthaEdit"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"fi.karppinen.Pyro"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"info.colloquy"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"kungfoo.tv.ecto"));

    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.apple.Dictionary"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.apple.Xcode"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.apple.helpviewer"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.culturedcode.xyle"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.macrabbit.CSSEdit"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.panic.Coda"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.ranchero.NetNewsWire"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.thinkmac.NewsLife"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"org.xlife.NewsFire"));
    EXPECT_EQ(WebCacheModelDocumentBrowser, TestWebPreferencesCacheModelForMainBundle(@"uk.co.opencommunity.vienna2"));

    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.app4mac.KidsBrowser"));
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.app4mac.wKiosk"));
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.freeverse.bumpercar"));
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.omnigroup.OmniWeb5"));
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, TestWebPreferencesCacheModelForMainBundle(@"com.sunrisebrowser.Sunrise"));
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, TestWebPreferencesCacheModelForMainBundle(@"net.hmdt-web.Shiira"));

    // Test bundle identifiers that are not hard-coded.
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(nil));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@""));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.apple.Safari"));
    EXPECT_EQ(WebCacheModelDocumentViewer, TestWebPreferencesCacheModelForMainBundle(@"com.apple.SafariTechnologyPreview"));
}

}
