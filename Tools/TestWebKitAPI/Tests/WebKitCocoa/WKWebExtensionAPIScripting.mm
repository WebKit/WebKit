/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKUserContentControllerPrivate.h>

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

static auto *changeBackgroundColorScript = @"document.body.style.background = 'pink'";
static auto *changeBackgroundFontScript = @"document.body.style.fontSize = '555px'";

TEST(WKWebExtensionAPIScripting, ErrorsExecuteScript)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.scripting.executeScript(), /a required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({}), /missing required keys: 'target'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target' : { }}), /missing required keys: 'tabId'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 'bad'}}), /'tabId' is expected to be a number, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, 'func': () => { console.log('function') }, 'function': () => {console.log('function')}}), /it cannot specify both 'func' and 'function'. Please use 'func'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: ['args'], func: () => 'function', arguments: ['arguments']}), /it cannot specify both 'args' and 'arguments'. Please use 'args'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: ['args'], files: ['path/to/file']}), /it must specify both 'func' and 'args'./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: 0, func: () => 'function' }), /'args' is expected to be an array, but a number was provided./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, func: () => 'function', args: [ () => 'arguments' ] }), /it is not JSON-serializable./i)",

        @"const notAFunction = null",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, func: 'not a function' }), /is expected to be a value, but a string was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }}), /it must specify either 'func' or 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, args: ['args'], files: [0]}), /'files' is expected to be an array of strings, but a number was provided./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1, allFrames: true, frameIds: [0] }, files: ['path/to/file']}), /it cannot specify both 'allFrames' and 'frameIds'./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1, frameIds: ['0'] }, files: ['path/to/file']}), /'frameIds' is expected to be an array of numbers, but a string was provided./i)",
        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1, frameIds: [-1] }, files: ['path/to/file']}), /'-1' is not a frame identifier./i)",

        @"browser.test.assertThrows(() => browser.scripting.executeScript({'target': { 'tabId': 1 }, world: 'world', files: ['path/to/file']}), /it must specify either 'isolated' or 'main'./i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(scriptingManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIScripting, ErrorsCSS)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.scripting.insertCSS(), /a required argument is missing./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({}), /missing required keys: 'target'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({ target: {} }), /missing required keys: 'tabId'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 1 }, files: ['path/to/file'], css: 'css'}), /it cannot specify both 'css' and 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 1 }}), /it must specify either 'css' or 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: '1' }, files: ['path/to/file'], css: 'css'}), /'tabId' is expected to be a number, but a string was provided./i)",
        @"browser.test.assertThrows(() => browser.scripting.insertCSS({target: { tabId: 1 }, css: 'body { color: red }', origin: 'bad' }), /'origin' value is invalid, because it must specify either 'AUTHOR' or 'USER'/i);",

        @"browser.test.assertThrows(() => browser.scripting.removeCSS(), /a required argument is missing./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({}), /missing required keys: 'target'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({ target: {} }), /missing required keys: 'tabId'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 1 }, files: ['path/to/file'], css: 'css'}), /it cannot specify both 'css' and 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 1 } }), /it must specify either 'css' or 'files'./i)",
        @"browser.test.assertThrows(() => browser.scripting.removeCSS({target: { tabId: 'bad' }, files: ['path/to/file'], css: 'css'}), /'tabId' is expected to be a number, but a string was provided./i)",

        @"browser.test.notifyPass()"
    ]);

    Util::loadAndRunExtension(scriptingManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPIScripting, ErrorsRegisteredContentScript)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts(), /a required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts({}), /an array is expected/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{}]), /it is missing required keys: 'id'/i)",

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: 0 }]), /'id' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '' }]), /it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '_0' }]), /it must not start with '_'/i)",

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0' }]), /it must specify at least one match pattern for script with ID '0'/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [] }]), /it must specify at least one match pattern for script with ID '0'/i)",

        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ] }]), /it must specify at least one 'css' or 'js' file/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], runAt: 'invalid_runAt' }]), /it must specify either 'document_start', 'document_end', or 'document_idle'/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], world: 'invalid_world' }]), /it must specify either 'isolated' or 'main'/i)",
        @"browser.test.assertThrows(() => browser.scripting.registerContentScripts([{ id: '0', matches: [ '*://*/*' ], css: ['style.css'], cssOrigin: 'bad' }]), /'cssOrigin' value is invalid, because it must specify either 'author' or 'user'/i)",

        @"browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '_0' }]), /it must not start with '_'/i)",
        @"browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '0', matches: [ ], js: ['path/to/file'] }]), /it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.scripting.updateContentScripts([{ id: '0', matches: [ '*://*/*' ], js: ['path/to/file'], world: 'invalid_world' }]), /it must specify either 'isolated' or 'main'/i)",

        @"browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts([]), /an object is expected/i)",
        @"browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts({ ids: 0 }), /'ids' is expected to be an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.scripting.getRegisteredContentScripts({ ids: [ 0 ] }), /'ids' is expected to be an array of strings, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.scripting.unregisterContentScripts([]), /an object is expected/i)",
        @"browser.test.assertThrows(() => browser.scripting.unregisterContentScripts({ ids: 0 }), /'ids' is expected to be an array of strings, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.scripting.unregisterContentScripts({ ids: [ 0 ] }), /'ids' is expected to be an array of strings, but a number was provided/i)",

        @"browser.test.assertRejects(() => browser.scripting.registerContentScripts([{ id: '1', matches: ['*://*/*'], js: ['invalidFile.js'] }]), /Invalid resource 'invalidFile.js'/i)",

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

        @"  function changeBackgroundColor(color) { document.body.style.background = color }",
        @"  function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",

        @"  let results = await browser.scripting.executeScript({ target: { tabId, allFrames: false }, files: [ 'backgroundColor.js' ] })",
        @"  browser.test.assertEq(results?.[0]?.result, 'pink', 'Result should be')",
        @"  browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')",

        @"  results = await browser.scripting.executeScript({ target: { tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.js' ] })",
        @"  browser.test.assertEq(results?.[0]?.result, 'pink', 'Result should be')",
        @"  browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')",

        @"  results = await browser.scripting.executeScript({ target: { tabId }, files: [ 'backgroundColor.js'] })",
        @"  browser.test.assertEq(results?.[0]?.result, 'pink', 'Result should be')",
        @"  browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')",

        @"  results = await browser.scripting.executeScript({ target: { tabId, frameIds: [ 0 ] }, func: changeBackgroundColor, args: ['pink'] })",
        @"  browser.test.assertEq(results?.[0]?.result, null, 'Result should be')",
        @"  browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')",

        @"  results = await browser.scripting.executeScript({",
        @"    target: { tabId },",
        @"    func: (bool, number, string, dict, array) => {",
        @"      browser.test.assertEq(typeof bool, 'boolean', 'Boolean argument should be')",
        @"      browser.test.assertEq(typeof number, 'number', 'Number argument should be')",
        @"      browser.test.assertEq(typeof string, 'string', 'String argument should be')",
        @"      browser.test.assertEq(typeof dict, 'object', 'Object argument should be')",
        @"      browser.test.assertTrue(Array.isArray(array), 'Array argument should be an array')",
        @"    },",
        @"    args: [true, 10, 'string', {}, []]",
        @"  })",
        @"  browser.test.assertEq(results?.[0]?.result, null, 'Result should be')",
        @"  browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')",

        @"  await browser.scripting.executeScript({ target: { tabId, allFrames: true }, func: changeBackgroundColor, args: ['blue'] })",
        @"  results = await browser.scripting.executeScript({ target: { tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results?.[0]?.result, blueValue, 'Result should be')",
        @"  browser.test.assertEq(results?.[0]?.frameId, 0, 'Frame should be')",

        @"  browser.test.assertSafeResolve(() => browser.scripting.executeScript({ target: { tabId, frameIds: [0], allFrames: false }, func: () => {} }))",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *backgroundColor = @"document.body.style.background = 'pink'";

    static auto *resources = @{ @"background.js": backgroundScript, @"backgroundColor.js": backgroundColor };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, ExecuteScriptJSONTypes)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const tabId = details.tabId",

        @"  const expectedResult = { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] }",

        @"  function returnJSONTypes() {",
        @"    return { 'boolean': true, 'number': 42, 'string': 'Test String', 'object': { 'key': 'value' }, 'array': [1, 2, 3] };",
        @"  }",

        @"  const results = await browser.scripting.executeScript({",
        @"    target: { tabId: tabId, allFrames: false },",
        @"    func: returnJSONTypes",
        @"  });",

        @"  browser.test.assertDeepEq(results?.[0]?.result, expectedResult);",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, ExecuteScriptWithFrameIds)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, " "_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Hello from frame')",

        @"  const frameId = sender.frameId",
        @"  browser.test.assertTrue(frameId !== 0, 'frameId should not be 0 for an iframe')",

        @"  const result = await browser.scripting.executeScript({",
        @"    target: { tabId: sender.tab.id, frameIds: [frameId] },",
        @"    func: () => location.pathname",
        @"  })",

        @"  browser.test.assertEq(result[0].result, '/frame.html', 'Should execute in the iframe and return the correct path')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.sendMessage('Hello from frame')",
    ]);

    static auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Scripting API Test",
        @"description": @"Scripting API Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module"
        },

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/frame.html" ],
            @"all_frames": @YES
        } ],

        @"permissions": @[ @"scripting" ]
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/frame.html"_s).URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, ExecuteScriptWithDocumentIds)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<span>Main Document</span><iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<span>Frame Document</span>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.runtime.onMessage.addListener(async (message, sender) => {",
        @"  browser.test.assertEq(message, 'Hello from frame')",

        @"  const documentId = sender?.documentId",
        @"  browser.test.assertEq(typeof documentId, 'string', 'sender.documentId should be')",
        @"  browser.test.assertEq(documentId.length, 36, 'sender.documentId should be')",

        @"  const result = await browser.scripting.executeScript({",
        @"    target: { tabId: sender?.tab?.id, documentIds: [ documentId ] },",
        @"    func: () => document.body.innerText.trim()",
        @"  })",

        @"  browser.test.assertEq(result?.[0]?.result, 'Frame Document', 'Result should be')",
        @"  browser.test.assertEq(typeof result?.[0]?.documentId, 'string', 'Result documentId should be')",
        @"  browser.test.assertEq(result?.[0]?.documentId.length, 36, 'Result documentId should be')",
        @"  browser.test.assertEq(result?.[0]?.documentId, documentId, 'Result documentId should be')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.runtime.sendMessage('Hello from frame')",
    ]);

    static auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Scripting API Test",
        @"description": @"Scripting API Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module"
        },

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/frame.html" ],
            @"all_frames": @YES
        } ],

        @"permissions": @[ @"scripting" ]
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/frame.html"_s).URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost()];

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

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status !== 'complete')",
        @"    return",

        @"  function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",
        @"  function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"  await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.css' ] })",
        @"  let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: false }, files: [ 'backgroundColor.css' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, transparentValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: false }, css: 'body { background-color: pink !important }' })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: false }, css: 'body { background-color: pink !important }' })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, transparentValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, pinkValue)",

        @"  await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, transparentValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        //   Storing original font size.
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: false }, func: getFontSize })",
        @"  let originalFontSize = results[0].result",

        @"  await browser.scripting.insertCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css', 'fontSize.css' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, pinkValue)",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getFontSize })",
        @"  browser.test.assertEq(results[0].result, '104px')",
        @"  browser.test.assertEq(results[1].result, '104px')",

        @"  await browser.scripting.removeCSS( { target: { tabId: tabId, allFrames: true }, files: [ 'backgroundColor.css', 'fontSize.css' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, transparentValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getFontSize })",
        @"  browser.test.assertEq(results[0].result, originalFontSize)",
        @"  browser.test.assertEq(results[1].result, originalFontSize)",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *backgroundColor = @"body { background-color: pink !important }";
    static auto *fontSize = @"body { font-size: 104px }";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"backgroundColor.css": backgroundColor,
        @"fontSize.css": fontSize,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
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

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status !== 'complete')",
        @"    return",

        @"  function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",
        @"  function getFontSize() { return window.getComputedStyle(document.body).getPropertyValue('font-size') }",

        @"  function logMessage() { console.log('Logging message') }",

        @"  await browser.scripting.insertCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.css' ] })",
        @"  let results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.removeCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, files: [ 'backgroundColor.css' ] })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, transparentValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.insertCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, css: 'body { background-color: pink !important }' })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, pinkValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        @"  await browser.scripting.removeCSS( { target: { tabId: tabId, frameIds: [ 0 ] }, css: 'body { background-color: pink !important }' })",
        @"  results = await browser.scripting.executeScript( { target: { tabId: tabId, allFrames: true }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results[0].result, transparentValue)",
        @"  browser.test.assertEq(results[1].result, blueValue)",

        // FIXME: <https://webkit.org/b/262491> Test with subframe once there's support for style injections for specific frame Ids.

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *backgroundColor = @"body { background-color: pink !important }";
    static auto *fontSize = @"body { font-size: 104px }";

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"backgroundColor.css": backgroundColor,
        @"fontSize.css": fontSize,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, CSSUserOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='color: red'></body>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status === 'complete') {",
        @"    await browser.test.assertSafeResolve(() => browser.scripting.insertCSS({ target: { tabId }, css: 'body { color: green !important }', origin: 'user' }))",

        @"    let results = await browser.test.assertSafeResolve(() => browser.scripting.executeScript({ target: { tabId }, func: () => getComputedStyle(document.body).color }))",
        @"    browser.test.assertEq(results[0]?.result, 'rgb(0, 128, 0)', 'Color should be green')",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, CSSAuthorOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<style> body { color: red } </style>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status === 'complete') {",
        @"    await browser.test.assertSafeResolve(() => browser.scripting.insertCSS({ target: { tabId }, css: 'body { color: green !important }', origin: 'author' }))",

        @"    let results = await browser.test.assertSafeResolve(() => browser.scripting.executeScript({ target: { tabId }, func: () => getComputedStyle(document.body).color }))",
        @"    browser.test.assertEq(results[0]?.result, 'rgb(0, 128, 0)', 'Color should be green')",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, World)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script> const world = 'MAIN'; </script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"function testWorld() {",
        @"  if (world)",
        @"    browser.test.notifyPass()",
        @"  else",
        @"    throw 'Variable not defined'",
        @"}",

        @"browser.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {",
        @"  if (tab.status === 'complete') {",
        @"    var results = await browser.scripting.executeScript( { target: { tabId, world: 'ISOLATED'}, func: testWorld })",
        @"    browser.test.assertEq(results[0]?.error, 'A JavaScript exception occurred')",

        @"    await browser.scripting.executeScript( { target: { tabId, world: 'MAIN'}, func: testWorld })",

        @"    browser.test.notifyPass()",
        @"  }",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
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
        @"    id: '1',",
        @"    js: ['changeBackgroundColorScript.js', 'changeBackgroundFontScript.js'],",
        @"    matches: ['*://localhost/*'],",
        @"    persistAcrossSessions: true,",
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

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, RegisterContentScriptsWithCSSUserOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='color: red'></body>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const tabId = details.tabId",

        @"  await browser.scripting.registerContentScripts([{",
        @"    id: 'changeColor',",
        @"    matches: ['*://localhost/*'],",
        @"    css: ['changeColor.css'],",
        @"    cssOrigin: 'user'",
        @"  }])",

        @"  let results = await browser.test.assertSafeResolve(() =>",
        @"    browser.scripting.executeScript({",
        @"      target: { tabId: tabId },",
        @"      func: () => getComputedStyle(document.body).color",
        @"    })",
        @"  )",

        @"  browser.test.assertEq(results[0].result, 'rgb(0, 128, 0)', 'Color should be green')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *css = @"body { color: green !important }";

    auto resources = @{
        @"background.js": backgroundScript,
        @"changeColor.css": css
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, RegisterContentScriptsWithCSSAuthorOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<style> body { color: red } </style>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const tabId = details.tabId",

        @"  await browser.scripting.registerContentScripts([{",
        @"    id: 'changeColor',",
        @"    matches: ['*://localhost/*'],",
        @"    css: ['changeColor.css'],",
        @"    cssOrigin: 'user'",
        @"  }])",

        @"  let results = await browser.test.assertSafeResolve(() =>",
        @"    browser.scripting.executeScript({",
        @"      target: { tabId: tabId },",
        @"      func: () => getComputedStyle(document.body).color",
        @"    })",
        @"  )",

        @"  browser.test.assertEq(results[0].result, 'rgb(0, 128, 0)', 'Color should be green')",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    auto *css = @"body { color: green !important }";

    auto resources = @{
        @"background.js": backgroundScript,
        @"changeColor.css": css
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

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
        @"    id: '1',",
        @"    js: ['changeBackgroundColorScript.js'],",
        @"    matches: ['*://localhost/*'],",
        @"    persistAcrossSessions: true,",
        @"  }]",

        @"  var results",

        @"  await browser.scripting.registerContentScripts([{ id: '1', matches: ['*://localhost/*'], js: ['changeBackgroundColorScript.js'] }])",

        @"  await browser.scripting.updateContentScripts([{ id: '1', allFrames: true, persistAcrossSessions: false, runAt: 'document_start', world: 'MAIN' }])",
        @"  results = await browser.scripting.getRegisteredContentScripts()",
        @"  expectedResults[0].allFrames = true",
        @"  expectedResults[0].persistAcrossSessions = false",
        @"  expectedResults[0].runAt = 'document_start'",
        @"  expectedResults[0].world = 'main'",

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

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

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
        @"    id: '1',",
        @"    js: ['changeBackgroundColorScript.js'],",
        @"    matches: ['*://localhost/*'],",
        @"    persistAcrossSessions: true,",
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
        @"  expectedResults.push({ id: '2', js: ['changeBackgroundFontScript.js'], matches: ['*://localhost/*'], persistAcrossSessions: true, runAt: 'document_start', world: 'main' })",
        @"  browser.test.assertDeepEq(results, expectedResults)",

        @"  await browser.scripting.unregisterContentScripts()",

        @"  browser.test.notifyPass()",
        @"})",

        @"browser.test.yield('Load Tab')",
    ]);

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

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

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
        @"changeBackgroundFontScript.js": changeBackgroundFontScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    auto *url = urlRequest.URL;

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithScheme:url.scheme host:url.host path:@"/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionWebNavigation];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, RegisteredScriptIsInjectedAfterContextReloads)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.webNavigation.onCompleted.addListener(async (details) => {",
        @"  const pinkValue = 'rgb(255, 192, 203)'",
        @"  function getBackgroundColor() { return window.getComputedStyle(document.body).getPropertyValue('background-color') }",

        @"  let results = await browser.scripting.executeScript({ target: { tabId: details.tabId, allFrames: false }, func: getBackgroundColor })",
        @"  browser.test.assertEq(results?.[0]?.result, pinkValue)",

        @"  browser.test.notifyPass()",
        @"})",

        @"let registeredScripts = await browser.scripting.getRegisteredContentScripts()",
        @"if (!registeredScripts.length) {",
        @"  await browser.scripting.registerContentScripts([{ id: '1', matches: [ '*://localhost/*' ], js: [ 'changeBackgroundColorScript.js' ] }])",

        @"  registeredScripts = await browser.scripting.getRegisteredContentScripts()",
        @"  browser.test.assertEq(registeredScripts.length, 1)",

        @"  browser.test.yield('Unload extension')",
        @"} else {",
        @"  browser.test.assertEq(registeredScripts.length, 1)",

        @"  browser.test.yield('Load Tab')",
        @"}"
    ]);

    static auto *resources = @{
        @"background.js": backgroundScript,
        @"changeBackgroundColorScript.js": changeBackgroundColorScript,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:scriptingManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get() extensionControllerConfiguration:WKWebExtensionControllerConfiguration._temporaryConfiguration]);

    // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
    manager.get().context.uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    EXPECT_FALSE(manager.get().context.hasInjectedContent);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Unload extension");

    EXPECT_TRUE(manager.get().context.hasInjectedContent);

    [manager.get().controller unloadExtensionContext:manager.get().context error:nullptr];

    EXPECT_FALSE(manager.get().context.hasInjectedContent);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    EXPECT_TRUE(manager.get().context.hasInjectedContent);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, MainWorld)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>window.secretValue = 'This is a secret.'</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScriptsManifest = @{
        @"manifest_version": @3,

        @"name": @"Scripting Test",
        @"description": @"Scripting Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content_script.js" ],
            @"world": @"MAIN"
        } ]
    };

    auto *contentScript = Util::constructScript(@[
        @"browser.test.assertEq(window.secretValue, 'This is a secret.')",

        // Externally connectable exposes a limited browser but not chrome.
        @"browser.test.assertEq(window.chrome, undefined)",
        @"browser.test.assertEq(typeof window.browser?.runtime, 'object')",
        @"browser.test.assertEq(window.browser?.runtime?.id, undefined)",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"content_script.js": contentScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:contentScriptsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIScripting, IsolatedWorld)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>window.secretValue = 'This is a secret.'</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScriptsManifest = @{
        @"manifest_version": @3,

        @"name": @"Scripting Test",
        @"description": @"Scripting Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content_script.js" ],
            @"world": @"ISOLATED"
        } ]
    };

    auto *contentScript = Util::constructScript(@[
        @"browser.test.assertEq(window.secretValue, undefined)",

        // Both chrome and browser should be exposed.
        @"browser.test.assertEq(typeof window.chrome?.runtime?.id, 'string')",
        @"browser.test.assertEq(typeof window.browser?.runtime?.id, 'string')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"content_script.js": contentScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:contentScriptsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPIScripting, RemoveAllUserScriptsDoesNotRemoveWebExtensionScripts)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScriptsManifest = @{
        @"manifest_version": @3,

        @"name": @"Scripting Test",
        @"description": @"Scripting Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content.js" ]
        } ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Remove Scripts and Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:contentScriptsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Remove Scripts and Load Tab");

    [manager.get().defaultTab.webView.configuration.userContentController removeAllUserScripts];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIScripting, RemoveAllUserStyleSheetsDoesNotRemoveWebExtensionStyleSheets)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<div id='test'>Test</div>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScriptsManifest = @{
        @"manifest_version": @3,

        @"name": @"Scripting Test",
        @"description": @"Scripting Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"css": @[ @"content.css" ],
            @"js": @[ @"content.js" ]
        } ]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Remove StyleSheets and Load Tab')"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"const testElement = document.getElementById('test')",
        @"const style = window.getComputedStyle(testElement)",
        @"browser.test.assertEq(style.color, 'rgb(255, 0, 0)', 'CSS should still apply after removing user stylesheets')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.css": @"#test { color: red; }",
        @"content.js": contentScript
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:contentScriptsManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Remove StyleSheets and Load Tab");

    [manager.get().defaultTab.webView.configuration.userContentController _removeAllUserStyleSheets];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
