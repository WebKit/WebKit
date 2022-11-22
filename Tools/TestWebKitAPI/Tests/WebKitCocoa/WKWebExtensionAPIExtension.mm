/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "WebExtensionUtilities.h"

namespace TestWebKitAPI {

TEST(WKWebExtensionAPIExtension, GetURL)
{
    auto *baseURLString = @"test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3";

    auto *backgroundScript = Util::constructScript(@[
        // Variable Setup
        [NSString stringWithFormat:@"const baseURL = '%@'", baseURLString],

        // Error Cases
        @"browser.test.assertThrows(() => browser.extension.getURL(), /A required argument is missing./)",
        @"browser.test.assertThrows(() => browser.extension.getURL(null), /Expected a string./)",
        @"browser.test.assertThrows(() => browser.extension.getURL(undefined), /Expected a string./)",

        // Normal Cases
        @"browser.test.assertEq(browser.extension.getURL(''), `${baseURL}/`)",
        @"browser.test.assertEq(browser.extension.getURL('test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.extension.getURL('/test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.extension.getURL('../../test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.extension.getURL('./test.js'), `${baseURL}/test.js`)",
        @"browser.test.assertEq(browser.extension.getURL('././/example'), `${baseURL}//example`)",
        @"browser.test.assertEq(browser.extension.getURL('../../example/..//test/'), `${baseURL}//test/`)",
        @"browser.test.assertEq(browser.extension.getURL('.'), `${baseURL}/`)",
        @"browser.test.assertEq(browser.extension.getURL('..//../'), `${baseURL}/`)",
        @"browser.test.assertEq(browser.extension.getURL('.././..'), `${baseURL}/`)",
        @"browser.test.assertEq(browser.extension.getURL('/.././.'), `${baseURL}/`)",

        // Unexpected Cases
        // FIXME: <https://webkit.org/b/248154> browser.extension.getURL() has some edge cases that should be failures or return different results
        @"browser.test.assertEq(browser.extension.getURL(42), `${baseURL}/42`)",
        @"browser.test.assertEq(browser.extension.getURL(/test/), `${baseURL}/test/`)",
        @"browser.test.assertEq(browser.extension.getURL('//'), 'test-extension://')",
        @"browser.test.assertEq(browser.extension.getURL('//example'), `test-extension://example`)",
        @"browser.test.assertEq(browser.extension.getURL('///'), 'test-extension:///')",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    auto *manifest = @{ @"manifest_version": @2, @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };
    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initWithExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];

    // Manifest v3 deprecates getURL(), so it should be an udefined property.

    backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.extension.getURL, 'undefined')",
        @"browser.test.assertEq(browser.extension.getURL, undefined)",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    manifest = @{ @"manifest_version": @3, @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO } };

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
