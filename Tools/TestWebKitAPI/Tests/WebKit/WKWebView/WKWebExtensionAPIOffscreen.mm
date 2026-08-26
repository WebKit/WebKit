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
        @"type": @"module"
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
        @"type": @"module"
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

TEST_F(WKWebExtensionAPIOffscreen, CreateAndHasDocument)
{
    auto *script = @[
        @"browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' }).then(async () => {",
        @"  browser.test.assertTrue(await browser.offscreen.hasDocument())",
        @"  browser.test.notifyPass()",
        @"})",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{
        @"background.js": Util::constructScript(script),
        @"offscreen.html": @"<!DOCTYPE html><html></html>",
    }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, CreateDocumentFails)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' }), /Offscreen document was closed/)",
        @"browser.test.assertFalse(await browser.offscreen.hasDocument())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{ @"background.js": Util::constructScript(script) }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, CreateDocumentTwiceFails)
{
    auto *script = @[
        @"await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })",
        @"await browser.test.assertRejects(browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' }), /Only a single offscreen document/)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{
        @"background.js": Util::constructScript(script),
        @"offscreen.html": @"<!DOCTYPE html><html></html>",
    }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, CloseDocument)
{
    auto *script = @[
        @"await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })",
        @"browser.test.assertTrue(await browser.offscreen.hasDocument())",
        @"await browser.offscreen.closeDocument()",
        @"browser.test.assertFalse(await browser.offscreen.hasDocument())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{
        @"background.js": Util::constructScript(script),
        @"offscreen.html": @"<!DOCTYPE html><html></html>",
    }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, CloseDocumentWithoutOneOpenFails)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.offscreen.closeDocument(), /No offscreen document is open/)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{ @"background.js": Util::constructScript(script) }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, HasDocumentFalseByDefault)
{
    auto *script = @[
        @"browser.test.assertFalse(await browser.offscreen.hasDocument())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{ @"background.js": Util::constructScript(script) }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, DocumentContentLoads)
{
    auto *script = @[
        @"browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })",
    ];

    auto *offscreenScript = @[
        @"browser.test.notifyPass()"
    ];

    Util::loadAndRunExtension(offscreenManifest, @{
        @"background.js": Util::constructScript(script),
        @"offscreen.html": @"<script type='module' src='offscreen.js'></script>",
        @"offscreen.js": Util::constructScript(offscreenScript),
    }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, SendMessageToDocument)
{
    auto *backgroundScript = @[
        @"await browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })",
        @"browser.runtime.sendMessage('Hello from background')",
    ];

    auto *offscreenScript = @[
        @"browser.runtime.onMessage.addListener((message) => {",
        @"  browser.test.assertEq(message, 'Hello from background', 'Should receive the expected message from the background page')",
        @"  browser.test.notifyPass()",
        @"})",
    ];

    Util::loadAndRunExtension(offscreenManifest, @{
        @"background.js": Util::constructScript(backgroundScript),
        @"offscreen.html": @"<script type='module' src='offscreen.js'></script>",
        @"offscreen.js": Util::constructScript(offscreenScript),
    }, offscreenConfig);
}

TEST_F(WKWebExtensionAPIOffscreen, OffscreenDocumentAPIAvailability)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.dom === undefined)",
        @"browser.test.assertFalse(browser.extension === undefined)",
        @"browser.test.assertFalse(browser.i18n === undefined)",
        @"browser.test.assertFalse(browser.runtime === undefined)",
        @"browser.test.assertFalse(browser.permissions === undefined)",
        @"browser.test.assertFalse(browser.tabs === undefined)",
        @"browser.test.assertFalse(browser.windows === undefined)",
        @"browser.offscreen.createDocument({ url: 'offscreen.html', reasons: ['TESTING'], justification: 'test' })",
    ];

    auto *offscreenScript = @[
        @"browser.test.assertFalse(browser.runtime === undefined)",

        @"browser.test.assertTrue(browser.dom === undefined)",
        @"browser.test.assertTrue(browser.extension === undefined)",
        @"browser.test.assertTrue(browser.i18n === undefined)",
        @"browser.test.assertTrue(browser.permissions === undefined)",
        @"browser.test.assertTrue(browser.tabs === undefined)",
        @"browser.test.assertTrue(browser.windows === undefined)",
        @"browser.test.notifyPass()"
    ];

    Util::loadAndRunExtension(offscreenManifest, @{
        @"background.js": Util::constructScript(script),
        @"offscreen.html": @"<script type='module' src='offscreen.js'></script>",
        @"offscreen.js": Util::constructScript(offscreenScript),
    }, offscreenConfig);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
