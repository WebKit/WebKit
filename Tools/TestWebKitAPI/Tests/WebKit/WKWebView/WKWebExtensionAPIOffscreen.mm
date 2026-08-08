/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>

namespace TestWebKitAPI {

#pragma mark - Constants

static auto *offscreenManifest = @{
    @"manifest_version": @3,
    @"name": @"Offscreen Test",
    @"description": @"Offscreen",
    @"version": @"1",

    @"permissions": @[ @"offscreen" ],
    @"background": @{
        @"service_worker": @"background.js",
    },
};

static auto *noOffscreenManifest = @{
    @"manifest_version": @3,
    @"name": @"No Offscreen Test",
    @"description": @"No Offscreen",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"service_worker": @"background.js",
    },
};

#pragma mark - Test Fixture

// This test fixture allows us to use offscreenConfig (which enables the offscreen feature flag) without manually constructing one on each run
class WKWebExtensionAPIOffscreen : public testing::Test {
protected:
    WKWebExtensionAPIOffscreen()
    {
        offscreenConfig = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
        if (!offscreenConfig.webViewConfiguration)
            offscreenConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in WKPreferences._features) {
            if ([feature.key isEqualToString:@"WebExtensionOffscreenEnabled"])
                [offscreenConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, offscreenConfig);
    }

    WKWebExtensionControllerConfiguration *offscreenConfig;
    RetainPtr<WKWebExtension> extension;
};

#pragma mark - Offscreen Tests

TEST_F(WKWebExtensionAPIOffscreen, APISUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.offscreen, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(noOffscreenManifest, @{ @"background.js": Util::constructScript(script) }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, OffscreenAPIAvailableWhenManifestRequests)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.offscreen === undefined)",
        @"browser.test.assertFalse(browser.offscreen.createDocument === undefined)",
        @"browser.test.assertFalse(browser.offscreen.closeDocument === undefined)",
        @"browser.test.assertFalse(browser.offscreen.hasDocument === undefined)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{ @"background.js": Util::constructScript(script) }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, OffscreenCreateDocumentArgumentValidation)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.offscreen === undefined)",
        @"browser.test.assertFalse(browser.offscreen.createDocument === undefined)",

        @"browser.test.assertThrows(() => browser.offscreen.createDocument(), /required argument is missing/)",

        // Only one argument specified (all three are required).
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'justification': 'test'}), /missing required keys: 'url' and 'reasons'/)",
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html'}), /missing required keys: 'justification' and 'reasons'/)",
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'reasons': ['Test']}), /missing required keys: 'url' and 'justification'/)",

        // Mising 'reasons'.
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 'test'}), /missing required keys: 'reasons'/)",

        // Missing 'justification'.
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'reasons': ['Test']}), /missing required keys: 'justification'/)",

        // Missing 'url'.
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'justification': 'test', 'reasons': ['Test']}), /missing required keys: 'url'/)",

        // Incorrect types for 'reasons'.
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 'test', 'reasons': 'Test'}), /'reasons' is expected to be an array of strings, but a string was provided/)",
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 'test', 'reasons': 5}), /'reasons' is expected to be an array of strings, but a number was provided/)",

        // Incorrect types for 'justification'.
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': ['test'], 'reasons': ['Test']}), /'justification' is expected to be a string, but an array was provided/)",
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 'test.html', 'justification': 5, 'reasons': ['Test']}), /'justification' is expected to be a string, but a number was provided/)",

        // Incorrect types for 'url'.
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': ['test.html'], 'justification': 'test', 'reasons': ['Test']}), /'url' is expected to be a string, but an array was provided/)",
        @"browser.test.assertThrows(() => browser.offscreen.createDocument({'url': 5, 'justification': 'test', 'reasons': ['Test']}), /'url' is expected to be a string, but a number was provided/)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{ @"background.js": Util::constructScript(script) }, offscreenConfig);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
