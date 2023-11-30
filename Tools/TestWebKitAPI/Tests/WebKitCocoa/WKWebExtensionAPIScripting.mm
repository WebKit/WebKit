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

static auto *scriptingManifest = @{
    @"manifest_version": @3,

    @"name": @"Scripting Test",
    @"description": @"Scripting Test",
    @"version": @"1",

    @"permissions": @[ @"scripting", @"webNavigation" ],
    @"host_permissions": @[ @"*://localhost/*" ],

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
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0 }, args: ['args'], files: [0]}), /'files' is expected to be an array of strings, but a number was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0, allFrames: true, frameIds: [0] }, files: ['path/to/file']}), /it cannot specify both 'allFrames' and 'frameIds'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 0, frameIds: ['0'] }, files: ['path/to/file']}), /'frameIds' is expected to be an array of numbers, but a string was provided./i)",
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

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts(), /a required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts({}), /an array is expected/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{}]), /it is missing required keys: 'id'/i)",

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: 0 }]), /'id' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '' }]), /it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '_0' }]), /it must not start with '_'/i)",

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0' }]), /it must specify at least one match pattern for script with ID '0'/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [] }]), /it must specify at least one match pattern for script with ID '0'/i)",

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ] }]), /it must specify at least one 'css' or 'js' file/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], runAt: 'invalid_runAt' }]), /it must be one of the following: 'document_end', 'document_idle', or 'document_start'/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], world: 'invalid_world' }]), /it must specify either 'ISOLATED' or 'MAIN'/i)",

        @"browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '_0' }]), /it must not start with '_'/i)",
        @"browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '0', matches: [ ], js: ['path/to/file'] }]), /it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], world: 'invalid_world' }]), /it must specify either 'ISOLATED' or 'MAIN'/i)",

        @"browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts([]), /an object is expected/i)",
        @"browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts({ ids: 0 }), /'ids' is expected to be an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts({ ids: [ 0 ] }), /'ids' is expected to be an array of strings, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.scripting.unregisterContentScripts([]), /an object is expected/i)",
        @"browser.test.assertThrows(() => browser.scripting.unregisterContentScripts({ ids: 0 }), /'ids' is expected to be an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.scripting.unregisterContentScripts({ ids: [ 0 ] }), /'ids' is expected to be an array of strings, but a number was provided/i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(scriptingManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIScripting, ExecuteScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const tabId = details.tabId",

        @"  const blueValue = 'rgb(0, 0, 255)'",

        @"  const expectedResultWithFileExecution = { 'result': 'pink', 'frameId': 0 }",
        @"  const expectedResultWithFunctionExecution = { 'result': null, 'frameId': 0 }",

        @"  function changeBackgroundColor(color) { document.body.style.background = color }",
        @"  function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",

        @"  let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.js' ] })",
        @"  browser.test.assertDeepEq(results[0], expectedResultWithFileExecution)",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.js' ] })",
        @"  browser.test.assertDeepEq(results[0], expectedResultWithFileExecution)",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId }, files: [ 'backgroundColor.js'] })",
        @"  browser.test.assertDeepEq(results[0], expectedResultWithFileExecution)",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ 0 ] }, func: changeBackgroundColor, args: ['pink'] })",
        @"  browser.test.assertDeepEq(results[0], expectedResultWithFunctionExecution)",

        @"  await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: changeBackgroundColor, args: ['blue'] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, blueValue)",
        @"  browser.test.assertEq(results[0].result, blueValue)",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *backgroundColor = @"document.body.style.background = 'pink'";

    static auto *resources = @{ @"background.js": backgroundScript, @"backgroundColor.js": backgroundColor };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, ExecuteScriptWithFrameIds)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  if (details.frameId !== 0)",
        @"    return",

        @"  const tabId = details.tabId",

        @"  const pinkValue = 'rgb(255, 192, 203)'",
        @"  const blueValue = 'rgb(0, 0, 255)'",

        @"  function changeBackgroundColor(color) { document.body.style.background = color }",
        @"  function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",
        @"  function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"  function logMessage() { console.log('Logging message') }",

        // FIXME: <https://webkit.org/b/260160> A better way to get the frame Ids would be from browser.webNavigation.getAllFrames().
        @"  let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: logMessage })",
        @"  browser.test.assertEq(results.length, 2)",

        @"  let subframe = results[1].frameId",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ 0, subframe ] }, files: [ 'backgroundColor.js' ]})",
        @"  browser.test.assertEq(results[0].result, 'pink')",
        @"  browser.test.assertEq(results[1].result, 'pink')",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ subframe ] }, func: changeBackgroundColor, args: [ 'blue' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ subframe ] }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, blueValue)",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ subframe ] }, files: [ 'backgroundColor.js', 'fontSize.js' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ subframe ] }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, frameIds: [ subframe ] }, func: getFontSize })",
        @"  browser.test.assertEq(results[0].result, '104px')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *backgroundColor = @"document.body.style.background = 'pink'";
    static auto *fontSize = @"document.body.style.fontSize = '104px'";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"backgroundColor.js": backgroundColor,
        @"fontSize.js": fontSize,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, InsertAndRemoveCSS)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const pinkValue = 'rgb(255, 192, 203)'",
        @"const blueValue = 'rgb(0, 0, 255)'",
        @"const transparentValue = 'rgba(0, 0, 0, 0)'",

        @"const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const tabId = tabs[0].id",

        @"function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",
        @"function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.css' ] })",
        @"let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, pinkValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.css' ] })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, transparentValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: false }, css: 'body { background-color: pink !important }' })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, pinkValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: false }, css: 'body { background-color: pink !important }' })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, transparentValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css' ] })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, pinkValue)",
        @"browser.test.assertEq(results[1].result, pinkValue)",

        @"await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css' ] })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, transparentValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        // Storing original font size.
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: false }, func: getFontSize })",
        @"let originalFontSize = results[0].result",

        @"await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css', 'fontSize.css' ] })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, pinkValue)",
        @"browser.test.assertEq(results[1].result, pinkValue)",

        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getFontSize })",
        @"browser.test.assertEq(results[0].result, '104px')",
        @"browser.test.assertEq(results[1].result, '104px')",

        @"await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css', 'fontSize.css' ] })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, transparentValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getFontSize })",
        @"browser.test.assertEq(results[0].result, originalFontSize)",
        @"browser.test.assertEq(results[1].result, originalFontSize)",

        @"browser.test.notifyPass()"
    ]);

    static auto *backgroundColor = @"body { background-color: pink !important }";
    static auto *fontSize = @"body { font-size: 104px }";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"backgroundColor.css": backgroundColor,
        @"fontSize.css": fontSize,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIScripting, InsertAndRemoveCSSWithFrameIds)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const pinkValue = 'rgb(255, 192, 203)'",
        @"const blueValue = 'rgb(0, 0, 255)'",
        @"const transparentValue = 'rgba(0, 0, 0, 0)'",

        @"const tabs = await browser.tabs.query({ active: true, currentWindow: true })",
        @"const tabId = tabs[0].id",

        @"function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",
        @"function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"function logMessage() { console.log('Logging message') }",

        @"await browser.scripting.insertCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.css' ] })",
        @"let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, pinkValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.removeCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.css' ] })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, transparentValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.insertCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, css: 'body { background-color: pink !important }' })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, pinkValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        @"await browser.scripting.removeCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, css: 'body { background-color: pink !important }' })",
        @"results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"browser.test.assertEq(results[0].result, transparentValue)",
        @"browser.test.assertEq(results[1].result, blueValue)",

        // FIXME: <https://webkit.org/b/262491> Test with subframe once there's support for style injections for specific frame Ids.

        @"browser.test.notifyPass()"
    ]);

    static auto *backgroundColor = @"body { background-color: pink !important }";
    static auto *fontSize = @"body { font-size: 104px }";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"backgroundColor.css": backgroundColor,
        @"fontSize.css": fontSize,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];
    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIScripting, RegisterContentScripts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",

        @"function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"  const pinkValue = 'rgb(255, 192, 203)'",

        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const tabId = details.tabId",

        @"  var expectedResults = [{",
        @"    allFrames: false,",
        @"    id: '1',",
        @"    js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'],",
        @"    matches: ['*://localhost/*'],",
        @"    persistAcrossSessions: true,",
        @"    runAt: 'document_idle',",
        @"    world: 'ISOLATED'",
        @"  }]",

        @"  var results",
        @"  var resultsPassingIds",

        @"  await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'] }])",
        @"  results = await browser.scripting.getRegisteredContentScripts()",
        @"  resultsPassingIds = await browser.scripting.getRegisteredContentScripts({ ids: ['1'] })",

        @"  browser.test.assertDeepEq(results, resultsPassingIds)",
        @"  browser.test.assertDeepEq(results, expectedResults)",

        // Verify scripts injected.

        @"  results = await browser.scripting.executeScript({ target: { tabId: tabId }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",

        @"  results = await browser.scripting.executeScript({ target: { tabId: tabId }, func: getFontSize })",
        @"  browser.test.assertEq(results[0].result, '555px')",

        @"  await browser.scripting.unregisterContentScripts()",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *changeBackgroundColorScript = @"document.body.style.background = 'pink'";
    static auto *changeBackgroundFontScript = @"document.body.style.fontSize = '555px'";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, UpdateContentScripts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",

        @"function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"  const pinkValue = 'rgb(255, 192, 203)'",

        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const tabId = details.tabId",

        @"  var expectedResults = [{",
        @"    allFrames: false,",
        @"    id: '1',",
        @"    js: ['changeBackgroundColorScript.js'],",
        @"    matches: ['*://localhost/*'],",
        @"    persistAcrossSessions: true,",
        @"    runAt: 'document_idle',",
        @"    world: 'ISOLATED'",
        @"  }]",

        @"  var results",

        @"  await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])",

        @"  await browser.scripting.updateContentScripts([{ id: '1', allFrames: true, persistAcrossSessions: false, runAt: 'document_start', world: 'MAIN' }])",
        @"  results = await browser.scripting.getRegisteredContentScripts()",
        @"  expectedResults[0].allFrames = true",
        @"  expectedResults[0].persistAcrossSessions = false",
        @"  expectedResults[0].runAt = 'document_start'",
        @"  expectedResults[0].world = 'MAIN'",

        @"  browser.test.assertDeepEq(results, expectedResults)",

        @"  await browser.scripting.updateContentScripts([{ id: '1', js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'] }])",
        @"  expectedResults[0].js = ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js']",
        @"  results = await browser.scripting.getRegisteredContentScripts()",

        @"  browser.test.assertDeepEq(results, expectedResults)",

        // Verify scripts injected.

        @"  results = await browser.scripting.executeScript({ target: { tabId: tabId, allFrames:true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, pinkValue)",

        @"  results = await browser.scripting.executeScript({ target: { tabId: tabId, allFrames:true }, func: getFontSize })",
        @"  browser.test.assertEq(results[0].result, '555px')",
        @"  browser.test.assertEq(results[1].result, '555px')",

        @"  await browser.scripting.unregisterContentScripts()",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *changeBackgroundColorScript = @"document.body.style.background = 'pink'";
    static auto *changeBackgroundFontScript = @"document.body.style.fontSize = '555px'";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, GetContentScripts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async () => {",

        @"  var expectedResults = [{",
        @"    allFrames: false,",
        @"    id: '1',",
        @"    js: ['changeBackgroundColorScript.js'],",
        @"    matches: ['*://localhost/*'],",
        @"    persistAcrossSessions: true,",
        @"    runAt: 'document_idle',",
        @"    world: 'ISOLATED'",
        @"  }]",

        @"  var results",

        @"  await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])",

        @"  results = await browser.scripting.getRegisteredContentScripts()",
        @"  browser.test.assertDeepEq(results, expectedResults)",

        @"  results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1'] })",
        @"  browser.test.assertDeepEq(results, expectedResults)",

        // Unrecognized ids should be ignored.
        @"  results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1', '2'] })",
        @"  browser.test.assertDeepEq(results, expectedResults)",

        @"  await browser.scripting.registerContentScripts([{ id: '2', matches: ['*://localhost/*'], js: ['changeBackgroundFontScript.js'], runAt: 'document_start', world: 'MAIN' }])",

        @"  results = await browser.scripting.getRegisteredContentScripts({ 'ids': ['1', '2'] })",
        @"  expectedResults.push({ allFrames: false, id: '2', js: ['changeBackgroundFontScript.js'], matches: ['*://localhost/*'], persistAcrossSessions: true, runAt: 'document_start', world: 'MAIN' })",
        @"  browser.test.assertDeepEq(results, expectedResults)",

        @"  await browser.scripting.unregisterContentScripts()",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *changeBackgroundColorScript = @"document.body.style.background = 'pink'";
    static auto *changeBackgroundFontScript = @"document.body.style.fontSize = '555px'";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, UnregisterContentScripts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",

        @"  const pinkValue = 'rgb(255, 192, 203)'",
        @"  const transparentValue = 'rgba(0, 0, 0, 0)'",

        @"  browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"    const tabId = details.tabId",

        @"    await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])",

        @"    var results",
        @"    results = await browser.scripting.getRegisteredContentScripts()",

        @"    browser.test.assertEq(results.length, 1)",

        @"    await browser.scripting.unregisterContentScripts()",
        @"    results = await browser.scripting.getRegisteredContentScripts()",
        @"    browser.test.assertEq(results.length, 0)",

        // Unrecognized ids should return an error and result in a no-op.

        @"    await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])",
        @"    browser.test.assertRejects(browser.scripting.unregisterContentScripts({ 'ids': ['1', '2'] }))",

        @"    results = await browser.scripting.executeScript({ target: { tabId: tabId }, func: getBackgroundColor })",
        @"    browser.test.assertEq(results[0].result, pinkValue)",

        // Tests removal with multiple ids.

        @"    await browser.scripting.registerContentScripts([{ id: '2', matches: ['*://localhost/*'], js: ['changeBackgroundFontScript.js'] }])",
        @"    results = await browser.scripting.getRegisteredContentScripts()",
        @"    browser.test.assertEq(results.length, 2)",

        @"    await browser.scripting.unregisterContentScripts({ 'ids': ['1', '2'] })",
        @"    results = await browser.scripting.getRegisteredContentScripts()",
        @"    browser.test.assertEq(results.length, 0)",

        @"    browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *changeBackgroundColorScript = @"document.body.style.background = 'pink'";
    static auto *changeBackgroundFontScript = @"document.body.style.fontSize = '555px'";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.mainWebView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
