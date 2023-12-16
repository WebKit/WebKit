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

#import "WebExtensionUtilities.h"
#import <WebKit/_WKWebExtensionCommand.h>

namespace TestWebKitAPI {

static auto *cookiesManifest = @{
    @"manifest_version": @3,

    @"name": @"Test Cookies",
    @"description": @"Test Cookies",
    @"version": @"1.0",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"permissions": @[ @"cookies" ],
};

TEST(WKWebExtensionAPICookies, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 123, name: 'Test' }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: '', name: 'Test' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'bad', name: 'Test' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'http://example.com', name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'http://example.com', name: '' }), /'name' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'http://example.com', storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.cookies.getAll({ url: 123 }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ url: '' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ url: 'bad' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ domain: 123 }), /'domain' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ path: 123 }), /'path' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ secure: 'bad' }), /'secure' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ session: 'bad' }), /'session' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.cookies.set({ url: 123 }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: '' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'bad' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', value: 123 }), /'value' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', domain: 123 }), /'domain' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', path: 123 }), /'path' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', secure: 'bad' }), /'secure' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', httpOnly: 'bad' }), /'httpOnly' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', sameSite: 123 }), /'sameSite' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', expirationDate: 'bad' }), /'expirationDate' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 123 }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: '', name: 'Test' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'bad', name: 'Test' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'http://example.com', name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'http://example.com', name: '' }), /'name' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'http://example.com', storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.notifyPass()",
    ]);

    Util::loadAndRunExtension(cookiesManifest, @{ @"background.js": backgroundScript });
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
