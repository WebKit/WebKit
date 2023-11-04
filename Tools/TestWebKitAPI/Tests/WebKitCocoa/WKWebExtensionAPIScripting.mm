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

namespace TestWebKitAPI {

static auto *manifest = @{
    @"manifest_version": @3,

    @"name": @"Scripting Test",
    @"description": @"Scripting Test",
    @"version": @"1",

    @"permissions": @[ @"scripting" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

TEST(WKWebExtensionAPIScripting, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.scripting.executeScript(), /a required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({}), /missing required keys: 'target'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target' : {}}), /missing required keys: 'tabId'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 'j'}}), /'tabId' is expected to be a number, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0}, 'func': () => { console.log('function') }, 'function': () => {console.log('function')}}), /it cannot specify both 'func' and 'function'. Please use 'func'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }, args: ['args'], func: () => 'function', arguments: ['arguments']}), /it cannot specify both 'args' and 'arguments'. Please use 'args'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }, args: ['args'], files: ['path/to/file']}), /it must specify both 'func' and 'args'./i)",

        @"const notAFunction = null",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }, func: 'not a function' }), /is expected to be a value, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }}), /it must specify either 'func' or 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }, args: ['args'], files: [0]}), /'files' is expected to be strings in an array, but a number was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0, allFrames: true, frameIds: [0] }, files: ['path/to/file']}), /it cannot specify both 'allFrames' and 'frameIds'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0, frameIds: ['0'] }, files: ['path/to/file']}), /'frameIds' is expected to be numbers in an array, but a string was provided./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0, frameIds: [-1] }, files: ['path/to/file']}), /'-1' is not a frame identifier./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }, world: 'world', files: ['path/to/file']}), /it must specify either 'ISOLATED' or 'MAIN'./i)",

        @"browser.test.assertThrows(() => browser.scripting.insertCSS(), /a required argument is missing./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({}), /missing required keys: 'target'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({ target: {} }), /missing required keys: 'tabId'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 0 }, files: ['path/to/file'], css: 'css'}), /it cannot specify both 'css' and 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 0 }}), /it must specify either 'css' or 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: '0' }, files: ['path/to/file'], css: 'css'}), /'tabId' is expected to be a number, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.removeCSS(), /a required argument is missing./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({}), /missing required keys: 'target'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({ target: {} }), /missing required keys: 'tabId'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 0 }, files: ['path/to/file'], css: 'css'}), /it cannot specify both 'css' and 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 0 } }), /it must specify either 'css' or 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: '0' }, files: ['path/to/file'], css: 'css'}), /'tabId' is expected to be a number, but a string was provided./i)",

        @"browser.test.notifyPass()"
    ]);

    // FIXME: <https://webkit.org/b/262491> Add additional tests for insertCSS() and removeCSS().

    Util::loadAndRunExtension(manifest, @{ @"background.js": backgroundScript });
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
