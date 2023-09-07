/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/_WKWebExtensionWindowCreationOptions.h>

namespace TestWebKitAPI {

static auto *tabsManifest = @{
    @"manifest_version": @3,

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

TEST(WKWebExtensionAPITabs, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.tabs.get('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.get(-3), /'tabID' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.duplicate('bad'), /'tabID' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.remove('bad'), /'tabIDs' value is invalid, because a number value or an array of number values is expected, but a string value was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.reload('bad'), /an unknown argument was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.goBack('bad'), /'tabID' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.goForward('bad'), /'tabID' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.getZoom('bad'), /'tabID' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.detectLanguage('bad'), /'tabID' value is invalid, because it is not a tab identifier/i)",
        @"browser.test.assertThrows(() => browser.tabs.toggleReaderMode('bad'), /'tabID' value is invalid, because it is not a tab identifier/i)",

        @"browser.test.assertThrows(() => browser.tabs.setZoom('bad'), /'zoomFactor' value is invalid, because a number is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.setZoom(1, 'bad'), /'zoomFactor' value is invalid, because a number is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.update('bad'), /'properties' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser.tabs.update(1, 'bad'), /'properties' value is invalid, because an object is expected/i)",

        @"browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': true }), /'openerTabId' is expected to be a number value, but a boolean value was provided/i)",
        @"browser.test.assertThrows(() => browser.tabs.update(1, { 'openerTabId': 4.2 }), /'openerTabId' value is invalid, because '4.2' is not a tab identifier/i)",

        @"browser.test.assertThrows(() => browser.tabs.create({ 'url': 1234 }), /'url' is expected to be a string value/i)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:tabsManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
