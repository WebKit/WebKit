/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

static auto *storageManifest = @{
    @"manifest_version": @3,

    @"name": @"Storage Test",
    @"description": @"Storage Test",
    @"version": @"1",

    @"permissions": @[ @"storage" ],
    @"host_permissions": @[ @"*://localhost/*" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

TEST(WKWebExtensionAPIStorage, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.storage.local.get({ 'key': () => { 'function' } }), /it is not JSON-serializable/i)",
        @"browser.test.assertThrows(() => browser.storage.local.get(Date.now()), /an invalid parameter was passed/i)",


        @"browser.test.assertThrows(() => browser.storage.local.getBytesInUse({}), /'keys' value is invalid, because a string or an array of strings or null is expected, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.storage.local.getBytesInUse([1]), /'keys' value is invalid, because a string or an array of strings or null is expected, but an array of other values was provided/i)",

        @"browser.test.assertThrows(() => browser.storage.local.set(), /A required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.storage.local.set([]), /'items' value is invalid, because an object is expected/i)",
        @"browser.test.assertThrows(() => browser.storage.local.set({ 'key': () => { 'function' } }), /it is not JSON-serializable/i)",

        @"browser.test.assertThrows(() => browser.storage.local.remove(), /A required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.storage.local.remove({}), /'keys' value is invalid, because a string or an array of strings is expected, but an object was provided/i)",
        @"browser.test.assertThrows(() => browser.storage.local.remove([1]), /'keys' value is invalid, because a string or an array of strings is expected, but an array of other values was provided/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(storageManifest, @{ @"background.js": backgroundScript });
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
