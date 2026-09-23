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

@interface WebPreferences (TestingInternal)
+ (WebCacheModel)_cacheModelForBundleIdentifier:(NSString *)bundleIdentifier;
@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, CacheModelForMainBundle)
{
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"Microsoft/com.microsoft.Messenger"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.adiumX.adiumX"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.alientechnology.Proteus"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.Dashcode"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.iChat"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.barebones.bbedit"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.barebones.textwrangler"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.barebones.yojimbo"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.equinux.iSale4"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.growl.growlframework"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.intrarts.PandoraMan"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.karelia.Sandvox"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.macromates.textmate"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.realmacsoftware.rapidweaverpro"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.red-sweater.marsedit"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.yahoo.messenger3"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"de.codingmonkeys.SubEthaEdit"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"fi.karppinen.Pyro"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"info.colloquy"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"kungfoo.tv.ecto"]);

    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.Dictionary"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.Xcode"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.helpviewer"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.culturedcode.xyle"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.macrabbit.CSSEdit"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.panic.Coda"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.ranchero.NetNewsWire"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.thinkmac.NewsLife"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"org.xlife.NewsFire"]);
    EXPECT_EQ(WebCacheModelDocumentBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"uk.co.opencommunity.vienna2"]);

    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.app4mac.KidsBrowser"]);
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.app4mac.wKiosk"]);
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.freeverse.bumpercar"]);
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.omnigroup.OmniWeb5"]);
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"com.sunrisebrowser.Sunrise"]);
    EXPECT_EQ(WebCacheModelPrimaryWebBrowser, [WebPreferences _cacheModelForBundleIdentifier:@"net.hmdt-web.Shiira"]);

    // Test bundle identifiers that are not hard-coded.
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:nil]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@""]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.Safari"]);
    EXPECT_EQ(WebCacheModelDocumentViewer, [WebPreferences _cacheModelForBundleIdentifier:@"com.apple.SafariTechnologyPreview"]);
}

}
