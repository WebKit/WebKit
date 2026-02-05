/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebExtensionDeclarativeNetRequestRule.h>
#import <WebKit/_WKWebExtensionDeclarativeNetRequestTranslator.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionAPIDeclarativeNetRequest, BlockedLoadTest)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Load Tab"];

    auto webView = manager.get().defaultTab.webView;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);
}

#if !defined(NDEBUG)
TEST(WKWebExtensionAPIDeclarativeNetRequest, DISABLED_BlockedLoadInPrivateBrowsingTest)
#else
TEST(WKWebExtensionAPIDeclarativeNetRequest, BlockedLoadInPrivateBrowsingTest)
#endif
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Load Tab"];

    manager.get().context.hasAccessToPrivateData = YES;
    [manager closeWindow:manager.get().defaultWindow];

    auto privateWindow = [manager openNewWindowUsingPrivateBrowsing:YES];
    auto privateTab = privateWindow.tabs.firstObject;
    auto webView = privateTab.webView;

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, GetEnabledRulesets)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')",
        @"browser.test.assertEq(enabledRulesets[0], 'blockFrame', 'blockFrame should have been enabled')",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                },
                @{
                    @"id": @"blockFrame2",
                    @"enabled": @NO,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UpdateEnabledRulesets)
{
    auto *backgroundScript = Util::constructScript(@[

        // Test invalid argument types
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: 5 }), /'options' value is invalid, because 'enableRulesetIds' is expected to be an array of strings/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: '5' }), /'options' value is invalid, because 'enableRulesetIds' is expected to be an array of strings/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: [5] }), /'options' value is invalid, because 'enableRulesetIds' is expected to be an array of strings/i)",

        @"browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: 5 }), /'options' value is invalid, because 'disableRulesetIds' is expected to be an array of strings/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: '5' }), /'options' value is invalid, because 'disableRulesetIds' is expected to be an array of strings/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: [5] }), /'options' value is invalid, because 'disableRulesetIds' is expected to be an array of strings/i)",

        // The given identifiers must be specified in the manifest.
        @"await browser.test.assertRejects(browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: ['notPresentIdentifier'] }), /Invalid ruleset id/i)",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.updateEnabledRulesets({ disableRulesetIds: ['notPresentIdentifier'] }), /Invalid ruleset id/i)",

        // Before any modifications
        @"let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')",
        @"browser.test.assertEq(enabledRulesets[0], 'blockFrame', 'blockFrame should have been enabled')",

        // Turn off `blockFrame` and turn on `blockFrame2`.
        @"await browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: ['blockFrame2'], disableRulesetIds: ['blockFrame'] })",

        // After the modifications
        @"enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')",
        @"browser.test.assertEq(enabledRulesets[0], 'blockFrame2', 'blockFrame2 should have been enabled')",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                },
                @{
                    @"id": @"blockFrame2",
                    @"enabled": @NO,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UpdateEnabledRulesetsPerformsCompilation)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        // Before any modifications
        @"let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 0, 'No rulesets should have been enabled')",

        // Turn on `blockFrame`.
        @"await browser.declarativeNetRequest.updateEnabledRulesets({ enableRulesetIds: ['blockFrame'] })",

        // After the modifications
        @"enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 1, 'One ruleset should have been enabled')",
        @"browser.test.assertEq(enabledRulesets[0], 'blockFrame', 'blockFrame should have been enabled')",

        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @NO,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Load Tab"];

    auto webView = manager.get().defaultTab.webView;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, IsRegexSupported)
{
    auto *backgroundScript = Util::constructScript(@[
        // Invalid arguments
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ }), /'regexOptions' value is invalid, because it is missing required keys: 'regex'/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: 5 }), /'regexOptions' value is invalid, because 'regex' is expected to be a string/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: ['.*'] }), /'regexOptions' value is invalid, because 'regex' is expected to be a string/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', isCaseSensitive: 'foo' }), /'regexOptions' value is invalid, because 'isCaseSensitive' is expected to be a boolean/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', isCaseSensitive: 5 }), /'regexOptions' value is invalid, because 'isCaseSensitive' is expected to be a boolean/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', requireCapturing: 'bar' }), /'regexOptions' value is invalid, because 'requireCapturing' is expected to be a boolean/i)",
        @"browser.test.assertThrows(() => browser.declarativeNetRequest.isRegexSupported({ regex: '.*', requireCapturing: 5 }), /'regexOptions' value is invalid, because 'requireCapturing' is expected to be a boolean/i)",

        // Passing cases
        @"browser.test.assertTrue((await browser.declarativeNetRequest.isRegexSupported({ regex: '.*' })).isSupported)",
        @"browser.test.assertTrue((await browser.declarativeNetRequest.isRegexSupported({ regex: 'a.*b' })).isSupported)",

        // Failing cases
        @"browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: 'Ä' })).isSupported)",
        @"browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: '' })).isSupported)",
        @"browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: 'a^' })).isSupported)",
        @"browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: 'this|that' })).isSupported)",
        @"browser.test.assertFalse((await browser.declarativeNetRequest.isRegexSupported({ regex: '$$' })).isSupported)",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, SetExtensionActionOptions)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"const [currentTab] = await browser.tabs.query({ active: true, currentWindow: true })",
        @"browser.declarativeNetRequest.setExtensionActionOptions({ displayActionCountAsBadgeText: true })",

        @"setTimeout(() => {",
        @"  browser.declarativeNetRequest.setExtensionActionOptions({ tabUpdate: { tabId: currentTab.id, increment: 2 } })",
        @"  browser.test.sendMessage('Check badge text')",
        @"}, 1000)",

        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"tabs" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest and tabs permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionTabs];

    [manager runUntilTestMessage:@"Load Tab"];

    auto *defaultTab = manager.get().defaultTab;
    auto *webView = defaultTab.webView;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);

    auto *action = [manager.get().context actionForTab:defaultTab];

    // The badge text should be "1" to match the one resource that was blocked.
    EXPECT_NS_EQUAL(action.badgeText, @"1");

    [manager runUntilTestMessage:@"Check badge text"];

    // The badge text should now be "3" since we incremented it by two.
    EXPECT_NS_EQUAL(action.badgeText, @"3");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, GetMatchedRules)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"setTimeout(async () => {",
        @"  const matchedRules = await browser.declarativeNetRequest.getMatchedRules()",
        @"  browser.test.assertEq(matchedRules.rulesMatchedInfo.length, 1)",
        @"  const matchedURL = matchedRules.rulesMatchedInfo[0].request.url",
        @"  browser.test.assertTrue(matchedURL.includes('localhost'), 'URL should include localhost')",
        @"  browser.test.assertTrue(matchedURL.includes('frame'), 'URL should include frame')",
        @"  browser.test.notifyPass()",
        @"}, 1000)",

        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequestFeedback permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, SessionRules)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"let sessionRules = await browser.declarativeNetRequest.getSessionRules()",
        @"browser.test.assertEq(sessionRules.length, 0)",
        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })",
        @"sessionRules = await browser.declarativeNetRequest.getSessionRules()",
        @"browser.test.assertEq(sessionRules.length, 1)",

        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    EXPECT_FALSE(manager.get().context.hasContentModificationRules);

    [manager runUntilTestMessage:@"Load Tab"];

    EXPECT_TRUE(manager.get().context.hasContentModificationRules);

    auto webView = manager.get().defaultTab.webView;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, GetSessionRules)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let sessionRules = await browser.declarativeNetRequest.getSessionRules()",
        @"browser.test.assertEq(sessionRules.length, 0)",

        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })",
        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })",
        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })",

        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: 1 }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: '' }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: true }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: { } }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: function foo() { } }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ '' ] }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ true ] }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ { } ] }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getSessionRules({ ruleIds: [ function foo() { } ] }))",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ })",
        @"sessionRules = sessionRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(sessionRules.length, 3)",
        @"browser.test.assertEq(sessionRules[0].id, 1)",
        @"browser.test.assertEq(sessionRules[1].id, 2)",
        @"browser.test.assertEq(sessionRules[2].id, 3)",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ ] })",
        @"sessionRules = sessionRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(sessionRules.length, 3)",
        @"browser.test.assertEq(sessionRules[0].id, 1)",
        @"browser.test.assertEq(sessionRules[1].id, 2)",
        @"browser.test.assertEq(sessionRules[2].id, 3)",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ 1 ] })",
        @"browser.test.assertEq(sessionRules.length, 1)",
        @"browser.test.assertEq(sessionRules[0].id, 1)",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ 1, 2 ] })",
        @"sessionRules = sessionRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(sessionRules.length, 2)",
        @"browser.test.assertEq(sessionRules[0].id, 1)",
        @"browser.test.assertEq(sessionRules[1].id, 2)",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ ruleIds: [ 1, 2, 3 ] })",
        @"sessionRules = sessionRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(sessionRules.length, 3)",
        @"browser.test.assertEq(sessionRules[0].id, 1)",
        @"browser.test.assertEq(sessionRules[1].id, 2)",
        @"browser.test.assertEq(sessionRules[2].id, 3)",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    Util::loadAndRunExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript  });
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RemoveSessionRules)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let sessionRules = await browser.declarativeNetRequest.getSessionRules()",
        @"browser.test.assertEq(sessionRules.length, 0)",

        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })",
        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })",
        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ })",
        @"browser.test.assertEq(sessionRules.length, 3)",

        @"await browser.declarativeNetRequest.updateSessionRules({ removeRuleIds: [1, 2, 3] })",

        @"sessionRules = await browser.declarativeNetRequest.getSessionRules({ })",
        @"browser.test.assertEq(sessionRules.length, 0)",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    Util::loadAndRunExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript  });
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, DynamicRules)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()",
        @"if (dynamicRules.length == 0) {",
        @"  await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })",
        @"  dynamicRules = await browser.declarativeNetRequest.getDynamicRules()",
        @"  browser.test.assertEq(dynamicRules.length, 1)",

        @"  browser.test.sendMessage('Unload extension')",
        @"} else {",
        @"  browser.test.assertEq(dynamicRules.length, 1)",

        @"  browser.test.sendMessage('Load Tab')",
        @"}"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto manager = Util::parseExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript }, WKWebExtensionControllerConfiguration._temporaryConfiguration);

    // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
    manager.get().context.uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    EXPECT_FALSE(manager.get().context.hasContentModificationRules);

    [manager load];
    [manager runUntilTestMessage:@"Unload extension"];

    EXPECT_TRUE(manager.get().context.hasContentModificationRules);

    auto *storageDirectory = manager.get().controller.configuration._storageDirectoryPath;
    storageDirectory = [storageDirectory stringByAppendingPathComponent:manager.get().context.uniqueIdentifier];
    EXPECT_TRUE([NSFileManager.defaultManager fileExistsAtPath:[storageDirectory stringByAppendingPathComponent:@"DeclarativeNetRequestContentRuleList.data"]]);

    [manager unload];
    [manager load];
    [manager runUntilTestMessage:@"Load Tab"];

    auto webView = manager.get().defaultTab.webView;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, GetDynamicRules)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()",
        @"browser.test.assertEq(dynamicRules.length, 0)",

        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })",
        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })",
        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })",

        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: 1 }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: '' }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: true }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: { } }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: function foo() { } }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ '' ] }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ true ] }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ { } ] }))",
        @"await browser.test.assertRejects(browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ function foo() { } ] }))",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ })",
        @"dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(dynamicRules.length, 3)",
        @"browser.test.assertEq(dynamicRules[0].id, 1)",
        @"browser.test.assertEq(dynamicRules[1].id, 2)",
        @"browser.test.assertEq(dynamicRules[2].id, 3)",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ ] })",
        @"dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(dynamicRules.length, 3)",
        @"browser.test.assertEq(dynamicRules[0].id, 1)",
        @"browser.test.assertEq(dynamicRules[1].id, 2)",
        @"browser.test.assertEq(dynamicRules[2].id, 3)",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ 1 ] })",
        @"browser.test.assertEq(dynamicRules.length, 1)",
        @"browser.test.assertEq(dynamicRules[0].id, 1)",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ 1, 2 ] })",
        @"dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(dynamicRules.length, 2)",
        @"browser.test.assertEq(dynamicRules[0].id, 1)",
        @"browser.test.assertEq(dynamicRules[1].id, 2)",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ ruleIds: [ 1, 2, 3 ] })",
        @"dynamicRules = dynamicRules.sort((a, b) => { a.id < b.id })",
        @"browser.test.assertEq(dynamicRules.length, 3)",
        @"browser.test.assertEq(dynamicRules[0].id, 1)",
        @"browser.test.assertEq(dynamicRules[1].id, 2)",
        @"browser.test.assertEq(dynamicRules[2].id, 3)",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    Util::loadAndRunExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript  });
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RemoveDynamicRules)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()",
        @"browser.test.assertEq(dynamicRules.length, 0)",

        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'foo' } }] })",
        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 2, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'bar' } }] })",
        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 3, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'baz' } }] })",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ })",
        @"browser.test.assertEq(dynamicRules.length, 3)",

        @"await browser.declarativeNetRequest.updateDynamicRules({ removeRuleIds: [1, 2, 3] })",

        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules({ })",
        @"browser.test.assertEq(dynamicRules.length, 0)",

        @"browser.test.notifyPass()"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    Util::loadAndRunExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript  });
}

static void runRedirectRule(bool useEnhancedSecurity)
{
    auto *pageScript = Util::constructScript(@[
        @"<script>",
        @"  browser.test.assertTrue(location.href.startsWith('http://127.0.0.1'), 'Final load should be via IP address')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, pageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    auto *redirectURL = server.request().URL.absoluteString;

    auto *rules = @[ @{
        @"id": @1,
        @"priority": @1,

        @"action": @{
            @"type": @"redirect",
            @"redirect": @{
                @"url": redirectURL
            }
        },

        @"condition": @{
            @"urlFilter": @"localhost",
            @"resourceTypes": @[ @"main_frame" ]
        }
    } ];

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },

        @"permissions": @[ @"declarativeNetRequestWithHostAccess" ],
        @"host_permissions": @[ @"*://localhost/*" ],

        @"declarative_net_request": @{
            @"rule_resources": @[ @{
                @"id": @"redirectRule",
                @"enabled": @YES,
                @"path": @"rules.json"
            } ]
        }
    };

    RetainPtr backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript.get(),
        @"rules.json": rules
    };

    auto extensionConfiguration = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;

    if (useEnhancedSecurity) {
        auto preferences = [extensionConfiguration.webViewConfiguration preferences];
        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"EnhancedSecurityHeuristicsEnabled"])
                [preferences _setEnabled:YES forFeature:feature];
        }
    }

    auto manager = Util::loadExtension(manifest, resources, extensionConfiguration, useEnhancedSecurity);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);

        if ([navigationAction.request.URL.absoluteString isEqual:redirectURL])
            EXPECT_TRUE(navigationAction.isContentRuleListRedirect);
    };

    manager.get().defaultTab.webView.navigationDelegate = navigationDelegate.get();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RedirectRule)
{
    runRedirectRule(false);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RedirectRuleWithEnhancedSecurity)
{
    runRedirectRule(true);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RedirectRuleWithoutHostAccessPermission)
{
    auto *pageScript = Util::constructScript(@[
        @"<script>",
        @"  browser.test.assertTrue(location.href.startsWith('http://localhost'), 'Final load should be via localhost')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, pageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    auto *redirectURL = server.request().URL.absoluteString;

    auto *rules = @[ @{
        @"id": @1,
        @"priority": @1,

        @"action": @{
            @"type": @"redirect",
            @"redirect": @{
                @"url": redirectURL
            }
        },

        @"condition": @{
            @"urlFilter": @"localhost",
            @"resourceTypes": @[ @"main_frame" ]
        }
    } ];

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },

        @"permissions": @[ @"declarativeNetRequestWithHostAccess" ],
        @"host_permissions": @[ @"*://localhost/*" ],

        @"declarative_net_request": @{
            @"rule_resources": @[ @{
                @"id": @"redirectRule",
                @"enabled": @YES,
                @"path": @"rules.json"
            } ]
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"rules.json": rules
    };

    auto manager = Util::loadExtension(manifest, resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RedirectRuleWithoutHostPermission)
{
    auto *pageScript = Util::constructScript(@[
        @"<script>",
        @"  browser.test.assertTrue(location.href.startsWith('http://localhost'), 'Final load should be via localhost')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, pageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();
    auto *redirectURL = server.request().URL.absoluteString;

    auto *rules = @[ @{
        @"id": @1,
        @"priority": @1,

        @"action": @{
            @"type": @"redirect",
            @"redirect": @{
                @"url": redirectURL
            }
        },

        @"condition": @{
            @"urlFilter": @"localhost",
            @"resourceTypes": @[ @"main_frame" ]
        }
    } ];

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },

        @"permissions": @[ @"declarativeNetRequestWithHostAccess" ],
        @"host_permissions": @[ @"*://localhost/*" ],

        @"declarative_net_request": @{
            @"rule_resources": @[ @{
                @"id": @"redirectRule",
                @"enabled": @YES,
                @"path": @"rules.json"
            } ]
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"rules.json": rules
    };

    auto manager = Util::loadExtension(manifest, resources);

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ModifyHeadersRule)
{
    auto *pageScript = Util::constructScript(@[
        @"<script>",
        @"  browser.test.assertEq(document.referrer, 'https://example.com/')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, pageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *rules = @[@{
        @"id": @1,
        @"priority": @1,

        @"action": @{
            @"type": @"modifyHeaders",
            @"requestHeaders": @[ @{
                @"header": @"Referer",
                @"operation": @"set",
                @"value": @"https://example.com/"
            } ]
        },

        @"condition": @{
            @"urlFilter": @"localhost",
            @"resourceTypes": @[ @"main_frame" ]
        }
    }];

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },

        @"permissions": @[ @"declarativeNetRequestWithHostAccess" ],
        @"host_permissions": @[ @"*://localhost/*" ],

        @"declarative_net_request": @{
            @"rule_resources": @[ @{
                @"id": @"modifyHeadersRule",
                @"enabled": @YES,
                @"path": @"rules.json"
            } ]
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"rules.json": rules
    };

    auto manager = Util::loadExtension(manifest, resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ModifyHeadersRuleWithoutHostAccessPermission)
{
    auto *pageScript = Util::constructScript(@[
        @"<script>",
        @"  browser.test.assertEq(document.referrer, '')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, pageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *rules = @[@{
        @"id": @1,
        @"priority": @1,

        @"action": @{
            @"type": @"modifyHeaders",
            @"requestHeaders": @[ @{
                @"header": @"Referer",
                @"operation": @"set",
                @"value": @"https://example.com/"
            } ]
        },

        @"condition": @{
            @"urlFilter": @"localhost",
            @"resourceTypes": @[ @"main_frame" ]
        }
    }];

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },

        @"permissions": @[ @"declarativeNetRequestWithHostAccess" ],
        @"host_permissions": @[ @"*://localhost/*" ],

        @"declarative_net_request": @{
            @"rule_resources": @[ @{
                @"id": @"modifyHeadersRule",
                @"enabled": @YES,
                @"path": @"rules.json"
            } ]
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"rules.json": rules
    };

    auto manager = Util::loadExtension(manifest, resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ModifyHeadersRuleWithoutHostPermission)
{
    auto *pageScript = Util::constructScript(@[
        @"<script>",
        @"  browser.test.assertEq(document.referrer, '')",

        @"  browser.test.notifyPass()",
        @"</script>"
    ]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, pageScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *rules = @[@{
        @"id": @1,
        @"priority": @1,

        @"action": @{
            @"type": @"modifyHeaders",
            @"requestHeaders": @[ @{
                @"header": @"Referer",
                @"operation": @"set",
                @"value": @"https://example.com/"
            } ]
        },

        @"condition": @{
            @"urlFilter": @"localhost",
            @"resourceTypes": @[ @"main_frame" ]
        }
    }];

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1",

        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },

        @"permissions": @[ @"declarativeNetRequestWithHostAccess" ],
        @"host_permissions": @[ @"*://localhost/*" ],

        @"declarative_net_request": @{
            @"rule_resources": @[ @{
                @"id": @"modifyHeadersRule",
                @"enabled": @YES,
                @"path": @"rules.json"
            } ]
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"rules.json": rules
    };

    auto manager = Util::loadExtension(manifest, resources);

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, MainFrameAllowAllRequests)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>browser.test.notifyPass()</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *rules = @"[ { \"id\" : 1, \"priority\": 2, \"action\" : { \"type\" : \"allowAllRequests\" }, \"condition\" : { \"urlFilter\" : \"*\", \"resourceTypes\" : [ \"main_frame\" ] } }, { \"id\" : 2, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockAndAllowRules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules  });

    // Grant the declarativeNetRequest permission.
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Load Tab"];

    auto webView = manager.get().defaultTab.webView;

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];

    [webView loadRequest:urlRequest];

    [manager run];
}

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugWithoutPermission)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(typeof browser.declarativeNetRequest, 'object')",
        @"browser.test.assertEq(typeof browser.declarativeNetRequest.onRuleMatchedDebug, 'undefined')",
        @"browser.test.notifyPass()"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });
    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebug)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
        { "/script.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script type='module' src='/script.js'></script>"_s } },
        { "/script.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, "browser.test.notifyFail('This script shouldn't load')"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    // FIXME: <rdar://159289161> Add checks for parentDocumentId once we support it
    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  switch (info.rule.rulesetId) {",
        @"    case 'sub_frame rules':",
        @"      browser.test.assertEq(typeof info.request.documentId, 'string')",
        @"      browser.test.assertEq(info.request.documentLifecycle, undefined)",
        @"      browser.test.assertTrue(info.request.frameId > 0)",
        @"      browser.test.assertEq(info.request.frameType, 'sub_frame')",
        // FIXME: <rdar://159231459> Initiator of a sub-frame is null; it should be src of the iframe.
        @"      browser.test.assertEq(info.request.initiator, undefined)",
        @"      browser.test.assertEq(info.request.method, 'GET')",
        @"      browser.test.assertTrue(info.request.parentFrameId > 0)",
        @"      browser.test.assertTrue(info.request.tabId > 0)",
        @"      browser.test.assertEq(info.request.type, 'sub_frame')",
        @"      browser.test.assertEq(new URL(info.request.url).pathname, '/frame.html')",

        @"      browser.test.assertEq(info.rule.ruleId, 1)",
        @"      browser.test.assertEq(info.rule.extensionId, undefined)",

        @"      browser.test.sendMessage('Done')",
        @"      break",
        @"    case 'main_frame rules':",
        @"      browser.test.assertEq(typeof info.request.documentId, 'string')",
        @"      browser.test.assertEq(info.request.documentLifecycle, undefined)",
        @"      browser.test.assertEq(info.request.frameId, 0)",
        @"      browser.test.assertEq(info.request.frameType, 'outermost_frame')",
        @"      browser.test.assertEq(info.request.initiator, 'localhost')",
        @"      browser.test.assertEq(info.request.method, 'GET')",
        @"      browser.test.assertEq(info.request.parentFrameId, -1)",
        @"      browser.test.assertTrue(info.request.tabId > 0)",
        @"      browser.test.assertEq(info.request.type, 'main_frame')",
        @"      browser.test.assertEq(new URL(info.request.url).pathname, '/frame.html')",

        @"      browser.test.assertEq(info.rule.ruleId, 2)",
        @"      browser.test.assertEq(info.rule.extensionId, undefined)",

        @"      browser.test.sendMessage('Done')",
        @"      break",
        @"    case 'non_frame rules':",
        @"      browser.test.assertEq(info.request.documentId, undefined)",
        @"      browser.test.assertEq(info.request.documentLifecycle, undefined)",
        @"      browser.test.assertEq(info.request.frameId, 0)",
        @"      browser.test.assertEq(info.request.frameType, undefined)",
        @"      browser.test.assertEq(info.request.initiator, 'localhost')",
        @"      browser.test.assertEq(info.request.method, 'GET')",
        @"      browser.test.assertEq(info.request.parentFrameId, -1)",
        @"      browser.test.assertTrue(info.request.tabId > 0)",
        @"      browser.test.assertEq(info.request.type, 'script')",
        @"      browser.test.assertEq(new URL(info.request.url).pathname, '/script.js')",

        @"      browser.test.assertEq(info.rule.ruleId, 3)",
        @"      browser.test.assertEq(info.rule.extensionId, undefined)",

        @"      browser.test.notifyPass()",
        @"      break",
        @"    default:",
        @"      browser.test.notifyFail('Received an unexpected rulesetId.')",
        @"  }",
        @"})",

        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *subFrameRules = @"[{\"id\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *mainFrameRules = @"[{\"id\":2,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"resourceTypes\":[\"main_frame\"]}}]";
    auto *nonFrameRules = @"[{\"id\":3,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"script.js\",\"resourceTypes\":[\"script\"]}}]";

    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"sub_frame rules",
                    @"enabled": @YES,
                    @"path": @"sub_frame.json"
                },
                @{
                    @"id": @"main_frame rules",
                    @"enabled": @YES,
                    @"path": @"main_frame.json"
                },
                @{
                    @"id": @"non_frame rules",
                    @"enabled": @YES,
                    @"path": @"non_frame.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"sub_frame.json": subFrameRules, @"main_frame.json": mainFrameRules, @"non_frame.json": nonFrameRules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    // Test sub_frame rules
    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager runUntilTestMessage:@"Done"];

    // Test main_frame rules
    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost("/frame.html"_s)];
    [manager runUntilTestMessage:@"Done"];

    // Test non_frame rules
    [manager.get().defaultTab.webView loadRequest:server.requestWithLocalhost("/script.html"_s)];
    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugSessionRules)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.assertEq(info.rule.rulesetId, '_session')",
        @"  browser.test.assertEq(info.rule.ruleId, 1)",
        @"  browser.test.notifyPass()",
        @"})",

        @"let sessionRules = await browser.declarativeNetRequest.getSessionRules()",
        @"browser.test.assertEq(sessionRules.length, 0)",
        @"await browser.declarativeNetRequest.updateSessionRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })",
        @"sessionRules = await browser.declarativeNetRequest.getSessionRules()",
        @"browser.test.assertEq(sessionRules.length, 1)",
        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugDynamicRules)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.assertEq(info.rule.rulesetId, '_dynamic')",
        @"  browser.test.assertEq(info.rule.ruleId, 1)",
        @"  browser.test.notifyPass()",
        @"})",

        @"let dynamicRules = await browser.declarativeNetRequest.getDynamicRules()",
        @"browser.test.assertEq(dynamicRules.length, 0)",
        @"await browser.declarativeNetRequest.updateDynamicRules({ addRules: [{ id: 1, priority: 1, action: {type: 'block'}, condition: { urlFilter: 'frame' } }] })",
        @"dynamicRules = await browser.declarativeNetRequest.getDynamicRules()",
        @"browser.test.assertEq(dynamicRules.length, 1)",
        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, DuplicatedRuleIDs)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 1, 'The static ruleset should be enabled.')",

        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"name": @"Test",
        @"description": @"Test dNR extension",
        @"version": @"1",
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"duplicated_rule_id",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"foo\" } }, { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"bar\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Load Tab"];

    NSArray<NSError *> *errors = manager.get().context.errors;
    ASSERT_EQ(errors.count, 1ul);
    ASSERT_TRUE([errors.firstObject.localizedDescription isEqualToString:@"`declarative_net_request` ruleset with id `duplicated_rule_id` duplicates the rule id `1`."]);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, DuplicatedRuleIDsInDifferentRulesets)
{
    auto *backgroundScript = Util::constructScript(@[
        @"let enabledRulesets = await browser.declarativeNetRequest.getEnabledRulesets()",
        @"browser.test.assertEq(enabledRulesets.length, 2, 'The static rulesets should be enabled.')",

        @"browser.test.sendMessage('Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"name": @"Test",
        @"description": @"Test dNR extension",
        @"version": @"1",
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"ruleset_1",
                    @"enabled": @YES,
                    @"path": @"rules1.json"
                },
                @{
                    @"id": @"ruleset_2",
                    @"enabled": @YES,
                    @"path": @"rules2.json"
                }
            ]
        }
    };

    auto *rules1 = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"foo\" } } ]";
    auto *rules2 = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"bar\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules1.json": rules1, @"rules2.json": rules2 });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Load Tab"];
    ASSERT_EQ(manager.get().context.errors.count, 0ul);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugExcludedRequestDomains)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded request domain.')",
        @"})",

        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"excludedRequestDomains\":[\"localhost\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager.get().defaultTab.webView _test_waitForDidFinishNavigation];
    [manager done];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugExcludedRequestMethods)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded request method.')",
        @"})",

        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"excludedRequestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager.get().defaultTab.webView _test_waitForDidFinishNavigation];
    [manager done];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugExcludedRequestDomainsAndExcludedRequestMethods)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded request domain or an excluded request method.')",
        @"})",

        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"excludedRequestDomains\":[\"localhost\"],\"excludedRequestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager.get().defaultTab.webView _test_waitForDidFinishNavigation];
    [manager done];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugRequestDomains)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.assertEq(info.rule.rulesetId, 'rules');",
        @"  browser.test.assertEq(info.rule.ruleId, 1);",
        @"  browser.test.assertEq(info.request.type, 'sub_frame');",
        @"  browser.test.notifyPass();",
        @"})",

        @"browser.test.sendMessage('Load Tab');",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"requestDomains\":[\"localhost\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugRequestMethods)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.assertEq(info.rule.rulesetId, 'rules');",
        @"  browser.test.assertEq(info.rule.ruleId, 1);",
        @"  browser.test.assertEq(info.request.type, 'sub_frame');",
        @"  browser.test.assertEq(info.request.method, 'GET');",
        @"  browser.test.notifyPass();",
        @"})",

        @"browser.test.sendMessage('Load Tab');",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"requestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugRequestDomainsAndRequestMethods)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.assertEq(info.rule.rulesetId, 'rules');",
        @"  browser.test.assertEq(info.rule.ruleId, 1);",
        @"  browser.test.assertEq(info.request.type, 'sub_frame');",
        @"  browser.test.assertEq(info.request.method, 'GET');",
        @"  browser.test.notifyPass();",
        @"})",
        @"browser.test.sendMessage('Load Tab');",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"requestDomains\":[\"localhost\"],\"requestMethods\":[\"get\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager run];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugRequestInitiatorDomainsAndExcludedInitiatorDomains)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  browser.test.notifyFail('onRuleMatchedDebug should not be called for an excluded initiator domain.')",
        @"})",

        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"block\"},\"condition\":{\"urlFilter\":\"frame\",\"initiatorDomains\":[\"example.com\"],\"excludedInitiatorDomains\":[\"localhost\"],\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager.get().defaultTab.webView _test_waitForDidFinishNavigation];
    [manager done];
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnRuleMatchedDebugRequestUpgradeScheme)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<h1>Hello, world!</h1>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"var onRuleMatchedDebugCount = 0",

        @"browser.declarativeNetRequest.onRuleMatchedDebug.addListener((info) => {",
        @"  onRuleMatchedDebugCount++",
        @"  browser.test.assertEq(onRuleMatchedDebugCount, 1, 'onRuleMatchedDebug should only be called once an upgrade action type.')",
        @"})",

        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *rules = @"[{\"id\":1,\"priority\":1,\"action\":{\"type\":\"upgradeScheme\"},\"condition\":{\"urlFilter\":\"frame\",\"resourceTypes\":[\"sub_frame\"]}}]";
    auto *manifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest", @"declarativeNetRequestFeedback" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"rules",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript, @"rules.json": rules });
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequestFeedback];
    [manager runUntilTestMessage:@"Load Tab"];

    auto *urlRequest = server.requestWithLocalhost();
    NSURL *requestURL = urlRequest.URL;
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:requestURL];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[requestURL URLByAppendingPathComponent:@"frame.html"]];
    [manager.get().defaultTab.webView loadRequest:urlRequest];
    [manager.get().defaultTab.webView _test_waitForDidFinishNavigation];
    [manager done];
}
#endif

// MARK: Rule translation tests

TEST(WKWebExtensionAPIDeclarativeNetRequest, RequiredAndOptionalKeys)
{
    NSDictionary *rule1 = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NOT_NULL(validatedRule1);

    NSDictionary *rule2 = @{
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule2);

    NSDictionary *rule3 = @{
        @"id": @1,
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule3 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule3 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule3);

    NSDictionary *rule4 = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule4 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule4 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule4);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, PropertiesHaveCorrectType)
{
    NSDictionary *rule1 = @{
        @"id": @"rule_id",
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule1);

    NSDictionary *rule2 = @{
        @"id": @1,
        @"priority": @1,
        @"action": @"block",
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };
    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule2);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NumbersArePositiveIntegers)
{
    NSDictionary *rule1 = @{
        @"id": @-1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule1);

    NSDictionary *rule2 = @{
        @"id": @1,
        @"priority": @0,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule2);

    NSDictionary *ruleWithNonIntegerPriority = @{
        @"id": @80,
        @"priority": @5.8,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRuleWithNonIntegerPriority = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:ruleWithNonIntegerPriority rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NOT_NULL(validatedRuleWithNonIntegerPriority);
    EXPECT_EQ(validatedRuleWithNonIntegerPriority.ruleID, 80);
    EXPECT_EQ(validatedRuleWithNonIntegerPriority.priority, 5);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, OnlyOneOfResourceTypesAndExcludedResourceTypesIsSpecified)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"boatnerd.com",
            @"resourceTypes": @[ @"font" ],
            @"excludedResourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RegexRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"regexFilter": @".*\\.com",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @".*\\.com",
            @"resource-type": @[ @"script" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, BasicValidRuleParsing)
{
    NSDictionary *conditionDictionary = @{
        @"urlFilter": @"crouton.net",
        @"resourceTypes": @[ @"script" ],
    };

    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @3,
        @"action": @{ @"type": @"block" },
        @"condition": conditionDictionary,
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NOT_NULL(validatedRule);
    EXPECT_EQ(validatedRule.ruleID, 1);
    EXPECT_EQ(validatedRule.priority, 3);
    EXPECT_NS_EQUAL(validatedRule.action, @{ @"type": @"block" });
    EXPECT_NS_EQUAL(validatedRule.condition, conditionDictionary);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, BasicRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"font" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, MainFrameResourceRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"top-document" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, SubFrameResourceRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"sub_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"child-document" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RepeatedMainFrameResourceRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"main_frame", @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"top-document" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, CaseSensitiveConversion)
{
    NSDictionary *rule1 = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"isUrlFilterCaseSensitive": @YES,
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule1 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule1 rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule1 = validatedRule1.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule1);

    NSDictionary *correctRuleConversion1 = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"url-filter-is-case-sensitive": @YES,
            @"resource-type": @[ @"font" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRule1, correctRuleConversion1);

    NSDictionary *rule2 = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"isUrlFilterCaseSensitive": @NO,
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule2 = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule2 rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule2 = validatedRule2.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule2);

    NSDictionary *correctRuleConversion2 = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"font" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRule2, correctRuleConversion2);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ConvertingMultipleResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script", @"stylesheet", @"ping" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSMutableDictionary *convertedRule = [validatedRule.ruleInWebKitFormat.firstObject mutableCopy];
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = [@{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": [@{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"script", @"style-sheet", @"ping" ],
        } mutableCopy],
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    } mutableCopy];
    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:correctRuleConversion[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);

    convertedRule[@"trigger"][@"resource-type"] = nil;
    correctRuleConversion[@"trigger"][@"resource-type"] = nil;
    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ConvertingXHRWebSocketAndOtherTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"xmlhttprequest", @"websocket", @"other" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSMutableDictionary *convertedRule = [validatedRule.ruleInWebKitFormat.firstObject mutableCopy];
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = [@{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": [@{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"fetch", @"websocket", @"other" ],
        } mutableCopy],
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    } mutableCopy];
    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:correctRuleConversion[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);

    convertedRule[@"trigger"][@"resource-type"] = nil;
    correctRuleConversion[@"trigger"][@"resource-type"] = nil;
    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UpgradeSchemeRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"upgradeScheme" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"image" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *makeHTTPSRule = @{
        @"action": @{
            @"type": @"make-https",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"image" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    NSDictionary *sortingRule = @{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"image" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRules[0], makeHTTPSRule);
    EXPECT_NS_EQUAL(convertedRules[1], sortingRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UpgradeSchemeForMainFrameRuleConversion)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"priority": @1,
        @"action": @{ @"type": @"upgradeScheme" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *makeHTTPSRule = @{
        @"action": @{
            @"type": @"make-https",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"top-document" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[0], makeHTTPSRule);

    NSDictionary *sortingRule = @{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": @{
            @"url-filter": @"crouton\\.net",
            @"resource-type": @[ @"top-document" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[1], sortingRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleWithoutAPriority)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"upgradeScheme" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NOT_NULL(validatedRule);
    EXPECT_EQ(validatedRule.priority, 1);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleWithInvalidDomainType)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"urlFilter": @"crouton.net",
            @"domainType": @"secondParty",
            @"resourceTypes": @[ @"ping" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithDomainType)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"boatnerd.com",
            @"resourceTypes": @[ @"script" ],
            @"domainType": @"firstParty",
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"url-filter": @"boatnerd\\.com",
            @"resource-type": @[ @"script" ],
            @"load-type": @[ @"first-party" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithNoSpecifiedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allow" },
        @"condition": @{
            @"regexFilter": @".*",
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSMutableDictionary *convertedRule = [validatedRule.ruleInWebKitFormat.firstObject mutableCopy];
    EXPECT_NOT_NULL(convertedRule);

    NSMutableDictionary *correctRuleConversion = [@{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": [@{
            @"url-filter": @".*",
            @"resource-type": @[ @"fetch", @"font", @"image", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket", @"child-document" ],
        } mutableCopy],
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    } mutableCopy];
    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:correctRuleConversion[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);

    convertedRule[@"trigger"][@"resource-type"] = nil;
    correctRuleConversion[@"trigger"][@"resource-type"] = nil;
    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithUnsupportedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"resourceTypes": @[ @"script", @"not_a_real_resource_type", @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:@[ @"script", @"font" ]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExactlyOneUnsupportedResourceType)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"resourceTypes": @[ @"not_a_real_resource_type" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NULL(convertedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, EmptyResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"resourceTypes": @[ ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"excludedResourceTypes": @[ @"main_frame", @"sub_frame", @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:@[ @"fetch", @"image", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket" ]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithUnsupportedExcludedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"*",
            @"excludedResourceTypes": @[ @"image", @"🦠" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSMutableDictionary *correctRuleConversion = [@{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": [@{
            @"url-filter": @".*",
            @"resource-type": @[ @"fetch", @"font", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket", @"top-document", @"child-document" ],
        } mutableCopy],
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    } mutableCopy];
    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:correctRuleConversion[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);

    convertedRule[@"trigger"][@"resource-type"] = nil;
    correctRuleConversion[@"trigger"][@"resource-type"] = nil;
    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithEmptyExcludedResourceTypes)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"*",
            @"excludedResourceTypes": @[ ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSMutableDictionary *convertedRule = [validatedRule.ruleInWebKitFormat.firstObject mutableCopy];
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = [@{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": [@{
            @"url-filter": @".*",
            @"resource-type": @[ @"fetch", @"font", @"image", @"media", @"other", @"ping", @"script", @"style-sheet", @"websocket", @"top-document", @"child-document" ],
        } mutableCopy],
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    } mutableCopy];

    NSSet *actualResourceTypes = [NSSet setWithArray:convertedRule[@"trigger"][@"resource-type"]];
    NSSet *expectedResourceTypes = [NSSet setWithArray:correctRuleConversion[@"trigger"][@"resource-type"]];
    EXPECT_NS_EQUAL(actualResourceTypes, expectedResourceTypes);

    convertedRule[@"trigger"][@"resource-type"] = nil;
    correctRuleConversion[@"trigger"][@"resource-type"] = nil;
    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, EmptyDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"domains": @[ ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"domains": @[ @"🙃🙃🙃", @"apple.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"domains": @[ @"apple.com", @"facebook.com", @"google.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"if-domain": @[ @"*apple.com", @"*facebook.com", @"*google.com" ],
            @"resource-type": @[ @"font" ],
            @"url-filter": @".*",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithURLFilterAndRequestDomains)
{
    __auto_type testPattern = ^(NSString *requestDomain, NSString *urlFilter, NSString *expectedRegexPattern) {
        NSMutableDictionary *rule = [@{
            @"id": @1,
            @"action": @{ @"type": @"block" },
            @"condition": [@{
                @"resourceTypes": @[ @"script" ],
                @"requestDomains": @[ requestDomain ],
            } mutableCopy],
        } mutableCopy];

        if (urlFilter)
            rule[@"condition"][@"urlFilter"] = urlFilter;

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": expectedRegexPattern,
                @"resource-type": @[ @"script" ],
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    // No URL filter
    testPattern(@"apple.com", nil, @"^[^:]+://+([^:/]+\\.)?apple\\.com");

    // URL filter contains the request domain
    testPattern(@"com", @".com/foo/bar", @"^[^:]+://+([^:/]+\\.)?\\.com/foo/bar");

    // Part of the request domain is in the URL filter
    testPattern(@"foo.com", @"bar-*.com", @"^[^:]+://+([^:/]+\\.)?bar-.*foo\\.com");
    testPattern(@"foo.com", @"foo.*/bar/baz", @"^[^:]+://+([^:/]+\\.)?foo\\.com.*/bar/baz");

    // || or :// prefix in the URL filter
    testPattern(@"com", @"||foo", @"^[^:]+://+([^:/]+\\.)?foo.*com");
    testPattern(@"com", @"://www.", @"://www\\..*com");
    testPattern(@"com", @"://www.*", @"://www\\..*com");
    testPattern(@"com", @"://www./foo/bar", @"://www\\..*com/foo/bar");
    testPattern(@"com", @"://www.*/foo/bar", @"://www\\..*com/foo/bar");

    // Concatenating the request domain and URL filter
    testPattern(@"com", @"/foo/bar", @"^[^:]+://+([^:/]+\\.)?com.*/foo/bar");
    testPattern(@"com", @"&foo=bar|", @"^[^:]+://+([^:/]+\\.)?com.*&foo=bar$");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithRequestDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            // Some extensions will have added leading * to match subdomains to work around Safari's bug in the past (rdar://113865040), make sure we still handle that case correctly.
            @"requestDomains": @[ @"apple.com", @"facebook.com", @"*google.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 3ul);

    NSDictionary *appleURLFilterRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"resource-type": @[ @"font" ],
            @"url-filter": @"^[^:]+://+([^:/]+\\.)?apple\\.com",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[0], appleURLFilterRuleConversion);

    NSDictionary *facebookURLFilterRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"resource-type": @[ @"font" ],
            @"url-filter": @"^[^:]+://+([^:/]+\\.)?facebook\\.com",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[1], facebookURLFilterRuleConversion);

    NSDictionary *googleURLFilterRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"resource-type": @[ @"font" ],
            @"url-filter": @"^[^:]+://+([^:/]+\\.)?.*google\\.com",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[2], googleURLFilterRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithInitiatorDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"initiatorDomains": @[ @"apple.com", @"facebook.com", @"google.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"if-frame-url": @[ @"^[^:]+://+([^:/]+\\.)?apple\\.com/.*", @"^[^:]+://+([^:/]+\\.)?facebook\\.com/.*", @"^[^:]+://+([^:/]+\\.)?google\\.com/.*" ],
            @"resource-type": @[ @"font" ],
            @"url-filter": @".*",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedInitiatorDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedInitiatorDomains": @[ @"apple.com", @"facebook.com", @"google.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"unless-frame-url": @[ @"^[^:]+://+([^:/]+\\.)?apple\\.com/.*", @"^[^:]+://+([^:/]+\\.)?facebook\\.com/.*", @"^[^:]+://+([^:/]+\\.)?google\\.com/.*" ],
            @"resource-type": @[ @"font" ],
            @"url-filter": @".*",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedInitiatorDomainsAndInitiatorDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"initiatorDomains": @[ @"example.com" ],
            @"excludedInitiatorDomains": @[ @"blog.example.com" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_NOT_NULL(convertedRules);

    NSArray *correctRuleConversion = @[
        @{
            @"action": @{
                @"type": @"ignore-following-rules",
            },
            @"trigger": @{
                @"if-frame-url": @[ @"^[^:]+://+([^:/]+\\.)?blog\\.example\\.com/.*" ],
                @"resource-type": @[ @"font" ],
                @"url-filter": @".*",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        },
        @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"if-frame-url": @[ @"^[^:]+://+([^:/]+\\.)?example\\.com/.*" ],
                @"resource-type": @[ @"font" ],
                @"url-filter": @".*",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        }
    ];

    EXPECT_NS_EQUAL(convertedRules, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithEmptyExcludedDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedDomains": @[ ],
            @"resourceTypes": @[ @"media" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"unless-domain": @[ ],
            @"resource-type": @[ @"media" ],
            @"url-filter": @".*",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIExcludedDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @".*",
            @"domains": @[ @"🧭" ],
            @"resourceTypes": @[ @"font" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedDomains": @[ @"apple.com" ],
            @"resourceTypes": @[ @"media" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"unless-domain": @[ @"*apple.com" ],
            @"resource-type": @[ @"media" ],
            @"url-filter": @".*",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedRequestDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedRequestDomains": @[ @"apple.com" ],
            @"resourceTypes": @[ @"media" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray<NSDictionary *> *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_EQ(convertedRules.count, 2ul);

    NSDictionary *passRuleConversion = @{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": @{
            @"resource-type": @[ @"media" ],
            @"url-filter": @"apple\\.com",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[0], passRuleConversion);

    NSDictionary *blockRuleConversion = @{
        @"action": @{
            @"type": @"block",
        },
        @"trigger": @{
            @"resource-type": @[ @"media" ],
            @"url-filter": @".*",
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };
    EXPECT_NS_EQUAL(convertedRules[1], blockRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithInvalidRequestMethods)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"requestMethods": @[ @"bad" ],
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithInvalidExcludedRequestMethods)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedRequestMethods": @[ @"bad" ],
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithRequestMethods)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"requestMethods": @[ @"get", @"post" ],
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_NOT_NULL(convertedRules);

    NSArray *correctRuleConversion = @[
        @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"top-document" ],
                @"request-method": @"get",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        },
        @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"top-document" ],
                @"request-method": @"post",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        }
    ];

    EXPECT_NS_EQUAL(convertedRules, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithExcludedRequestMethods)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"excludedRequestMethods": @[ @"get", @"post" ],
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_NOT_NULL(convertedRules);

    NSArray *correctRuleConversion = @[
        @{
            @"action": @{
                @"type": @"ignore-following-rules",
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"top-document" ],
                @"request-method": @"get",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        },
        @{
            @"action": @{
                @"type": @"ignore-following-rules",
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"top-document" ],
                @"request-method": @"post",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        },
        @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"top-document" ],
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        }
    ];

    EXPECT_NS_EQUAL(convertedRules, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithRequestMethodsAndExcludedRequestMethodsAndRequestDomainsAndExcludedRequestDomains)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"requestDomains": @[ @"apple.com" ],
            @"excludedRequestDomains": @[ @"google.com" ],
            @"requestMethods": @[ @"get" ],
            @"excludedRequestMethods": @[ @"post" ],
            @"resourceTypes": @[ @"main_frame" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSArray *convertedRules = validatedRule.ruleInWebKitFormat;
    EXPECT_NOT_NULL(convertedRules);

    NSArray *correctRuleConversion = @[
        @{
            @"action": @{
                @"type": @"ignore-following-rules",
            },
            @"trigger": @{
                @"url-filter": @"google\\.com",
                @"resource-type": @[ @"top-document" ],
                @"request-method": @"post",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        },
        @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": @"^[^:]+://+([^:/]+\\.)?apple\\.com",
                @"resource-type": @[ @"top-document" ],
                @"request-method": @"get",
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        }
    ];

    EXPECT_NS_EQUAL(convertedRules, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIURLFilter)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"urlFilter": @"🔮.com",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NonASCIIRegexFilter)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"block" },
        @"condition": @{
            @"regexFilter": @"〽️.com",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, URLFilterSpecialCharacters)
{
    __auto_type testPattern = ^(NSString *chromePattern, NSString *expectedRegexPattern) {
        NSDictionary *rule = @{
            @"id": @1,
            @"action": @{ @"type": @"block" },
            @"condition": @{
                @"urlFilter": chromePattern,
                @"resourceTypes": @[ @"script" ],
            },
        };

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"block",
            },
            @"trigger": @{
                @"url-filter": expectedRegexPattern,
                @"resource-type": @[ @"script" ],
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    // Testing domain anchor.
    testPattern(@"||apple.com", @"^[^:]+://+([^:/]+\\.)?apple\\.com");
    testPattern(@"apple||.com", @"apple\\|\\|\\.com");

    // Testing start and end anchors.
    testPattern(@"|apple|.com|", @"^apple\\|\\.com$");
    testPattern(@"|apple.com", @"^apple\\.com");
    testPattern(@"apple.com|", @"apple\\.com$");
    testPattern(@"apple|com", @"apple\\|com");

    // Testing wildcard.
    testPattern(@"*apple.com", @".*apple\\.com");
    testPattern(@"*apple*com", @".*apple.*com");
    testPattern(@"apple.com*", @"apple\\.com.*");

    // Testing separator character.
    testPattern(@"apple^com", @"apple[^a-zA-Z0-9_.%-]com");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, UnacceptableResourceTypeForAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
            @"resourceTypes": @[ @"main_frame", @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, ExcludedResourceTypeForAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
            @"excludedResourceTypes": @[ @"sub_frame", @"stylesheet", @"script", @"image", @"font", @"object", @"xmlhttprequest", @"ping", @"csp_report", @"media", @"websocket", @"other"],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, NoResourceTypeForAllowAllRequests)
{
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    EXPECT_NULL(validatedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithMainFrameAllowAllRequests)
{
    // FIXME: rdar://154124673 (Write tests for sub_frame allowAllRequests rules)
    NSDictionary *rule = @{
        @"id": @1,
        @"action": @{ @"type": @"allowAllRequests" },
        @"condition": @{
            @"urlFilter": @"apple.com",
            @"resourceTypes": @[ @"main_frame" ]
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSMutableDictionary *convertedRule = [validatedRule.ruleInWebKitFormat.firstObject mutableCopy];
    EXPECT_NOT_NULL(convertedRule);

    NSMutableDictionary *correctRuleConversion = [@{
        @"action": @{
            @"type": @"ignore-following-rules",
        },
        @"trigger": [@{
            @"url-filter": @".*",
            @"if-top-url": @[ @"apple\\.com" ],
        } mutableCopy],
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @1,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    } mutableCopy];

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithRedirect)
{
    __auto_type testRedirect = ^(NSDictionary<NSString *, id> *inputRedirect, NSDictionary<NSString *, id> *expectedRedirect) {
        NSDictionary *rule = @{
            @"id": @1,
            @"action": @{
                @"type": @"redirect",
                @"redirect": inputRedirect,
            },
            @"condition": @{
                @"regexFilter": @".*",
                @"resourceTypes": @[ @"script" ],
            },
        };

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;

        if (!expectedRedirect) {
            EXPECT_NULL(convertedRule);
            return;
        }

        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"redirect",
                @"redirect": expectedRedirect,
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"script" ],
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @1,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    testRedirect(@{ }, nil);

    testRedirect(@{ @"url": @"" }, nil);
    testRedirect(@{ @"url": @[ ] }, nil);
    testRedirect(@{ @"url": @"ftp://example.com" }, nil);
    testRedirect(@{ @"url": @"https://example.com" }, @{ @"url": @"https://example.com" });

    testRedirect(@{ @"extensionPath": @"" }, nil);
    testRedirect(@{ @"extensionPath": @[ ] }, nil);
    testRedirect(@{ @"extensionPath": @"foo.js" }, nil);
    testRedirect(@{ @"extensionPath": @"/foo.js" }, @{ @"extension-path": @"/foo.js" });

    testRedirect(@{ @"regexSubstitution": @"" }, nil);
    testRedirect(@{ @"regexSubstitution": @[ ] }, nil);
    testRedirect(@{ @"regexSubstitution": @"http://example.com/\\1" }, @{ @"regex-substitution": @"http://example.com/\\1" });

    testRedirect(@{ @"transform": @"" }, nil);
    testRedirect(@{ @"transform": @[ ] }, nil);
    testRedirect(@{ @"transform": @{ } }, nil);
    testRedirect(@{ @"transform": @{ @"scheme": @"https", @"host": @"new.example.com" } }, @{ @"transform": @{ @"scheme": @"https", @"host": @"new.example.com" } });
    testRedirect(@{ @"transform": @{ @"username": @"foo", @"password": @"bar" } }, @{ @"transform": @{ @"username": @"foo", @"password": @"bar" } });
    testRedirect(@{ @"transform": @{ @"query": @"foo" } }, @{ @"transform": @{ @"query": @"foo" } });

    testRedirect(@{ @"transform": @{ @"queryTransform": @"" } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @[ ] } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @"" } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ } ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"key": @"foo" } ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"replaceOnly": @"" } ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"removeParams": @"" } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"removeParams": @[ ] } } }, nil);
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"key": @"foo", @"value": @"bar" } ] } } }, @{ @"transform": @{ @"query-transform": @{ @"add-or-replace-parameters": @[ @{ @"key": @"foo", @"value": @"bar" } ] } } });
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"addOrReplaceParams": @[ @{ @"key": @"foo", @"value": @"bar", @"replaceOnly": @YES } ] } } }, @{ @"transform": @{ @"query-transform": @{ @"add-or-replace-parameters": @[ @{ @"key": @"foo", @"value": @"bar", @"replace-only": @YES } ] } } });
    testRedirect(@{ @"transform": @{ @"queryTransform": @{ @"removeParams": @[ @"foo" ] } } }, @{ @"transform": @{ @"query-transform": @{ @"remove-parameters": @[ @"foo" ] } } });
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithModifyHeaders)
{
    __auto_type testModifyHeaders = ^(NSString *inputHeaderType, NSArray<NSDictionary *> *inputModifyHeadersInfo, NSString *expectedHeaderType, NSArray<NSDictionary *> *expectedModifyHeadersInfo) {
        NSDictionary *rule = @{
            @"id": @10,
            @"priority": @2,
            @"action": @{
                @"type": @"modifyHeaders",
                inputHeaderType: inputModifyHeadersInfo,
            },
            @"condition": @{
                @"urlFilter": @"*",
                @"resourceTypes": @[ @"script" ],
            },
        };

        _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
        NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;

        if (!expectedModifyHeadersInfo) {
            EXPECT_NULL(convertedRule);
            return;
        }

        EXPECT_NOT_NULL(convertedRule);

        NSDictionary *correctRuleConversion = @{
            @"action": @{
                @"type": @"modify-headers",
                @"priority": @2,
                expectedHeaderType: expectedModifyHeadersInfo,
            },
            @"trigger": @{
                @"url-filter": @".*",
                @"resource-type": @[ @"script" ],
            },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
            @"_identifier": @10,
            @"_rulesetIdentifier": @"Test Ruleset",
#endif
        };

        EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
    };

    testModifyHeaders(@"responseHeaders", @[ ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ } ], nil, nil);
    testModifyHeaders(@"requestHeaders", @[ @{ } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"wrong dictionary structure": @[ @{ } ] } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"invalid header operation", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"operation": @"set", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"set" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"append" } ], nil, nil);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"remove", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"requestHeaders", @[ @{ @"operation": @"set", @"value": @"v1" } ], nil, nil);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"ellie's custom header", @"operation": @"set", @"value": @"v1" } ], nil, nil);

    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ], @"request-headers", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ]);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"remove" } ],  @"request-headers", @[ @{ @"header": @"accept", @"operation": @"remove" } ]);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" } ], @"request-headers", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" } ]);
    testModifyHeaders(@"requestHeaders", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" }, @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ], @"request-headers", @[ @{ @"header": @"accept", @"operation": @"append", @"value": @"v1" }, @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ]);
    testModifyHeaders(@"responseHeaders", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ], @"response-headers", @[ @{ @"header": @"accept", @"operation": @"set", @"value": @"v1" } ]);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithModifyHeadersWithBothResponseAndRequestHeaders)
{
    NSDictionary *rule = @{
        @"id": @10,
        @"action": @{
            @"type": @"modifyHeaders",
            @"responseHeaders": @[
                @{ @"header": @"age", @"operation": @"set", @"value": @"5" },
                @{ @"header": @"date", @"operation": @"remove" },
            ],
            @"requestHeaders": @[
                @{ @"header": @"cache-control", @"operation": @"set", @"value": @"something" },
                @{ @"header": @"Cookie", @"operation": @"remove" },
            ],
        },
        @"condition": @{
            @"urlFilter": @"*",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NOT_NULL(convertedRule);

    NSDictionary *correctRuleConversion = @{
        @"action": @{
            @"type": @"modify-headers",
            @"priority": @1,
            @"response-headers": @[
                @{ @"header": @"age", @"operation": @"set", @"value": @"5" },
                @{ @"header": @"date", @"operation": @"remove" },
            ],
            @"request-headers": @[
                @{ @"header": @"cache-control", @"operation": @"set", @"value": @"something" },
                @{ @"header": @"Cookie", @"operation": @"remove" },
            ],
        },
        @"trigger": @{
            @"url-filter": @".*",
            @"resource-type": @[ @"script" ],
        },
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
        @"_identifier": @10,
        @"_rulesetIdentifier": @"Test Ruleset",
#endif
    };

    EXPECT_NS_EQUAL(convertedRule, correctRuleConversion);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RuleConversionWithModifyHeadersWithoutAnyHeadersSpecified)
{
    NSDictionary *rule = @{
        @"id": @10,
        @"priority": @2,
        @"action": @{
            @"type": @"modifyHeaders",
        },
        @"condition": @{
            @"urlFilter": @"*",
            @"resourceTypes": @[ @"script" ],
        },
    };

    _WKWebExtensionDeclarativeNetRequestRule *validatedRule = [[_WKWebExtensionDeclarativeNetRequestRule alloc] initWithDictionary:rule rulesetID:@"Test Ruleset" errorString:nil];
    NSDictionary *convertedRule = validatedRule.ruleInWebKitFormat.firstObject;
    EXPECT_NULL(convertedRule);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RulesSortByPriorityFromDifferentRulesets)
{
    NSDictionary *rules = @{
        @"Test Ruleset 1": @[ @{
            @"id": @1,
            @"priority": @2,
            @"action": @{ @"type": @"allow" },
            @"condition": @{
                @"regexFilter": @"apple.com",
                @"resourceTypes": @[ @"script" ],
            },
        } ],
        @"Test Ruleset 2": @[ @{
            @"id": @1,
            @"priority": @1,
            @"action": @{ @"type": @"block" },
            @"condition": @{
                @"regexFilter": @"bananas.com",
                @"resourceTypes": @[ @"script" ],
            },
        } ],
    };

    NSArray *sortedTranslatedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:rules errorStrings:nil];
    EXPECT_NOT_NULL(sortedTranslatedRules);

    EXPECT_NS_EQUAL(sortedTranslatedRules[0][@"action"][@"type"], @"ignore-following-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[1][@"action"][@"type"], @"block");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RulesSortWithoutExplicitPriority)
{
    NSDictionary *rules = @{
        @"Test Ruleset": @[
            @{
                @"id": @1,
                @"action": @{ @"type": @"upgradeScheme" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"priority": @3,
                @"action": @{ @"type": @"block" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
        ]
    };

    NSArray *sortedTranslatedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:rules errorStrings:nil];
    EXPECT_NOT_NULL(sortedTranslatedRules);

    EXPECT_NS_EQUAL(sortedTranslatedRules[0][@"action"][@"type"], @"block");
    EXPECT_NS_EQUAL(sortedTranslatedRules[1][@"action"][@"type"], @"make-https");
    EXPECT_NS_EQUAL(sortedTranslatedRules[2][@"action"][@"type"], @"ignore-following-rules");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RulesSortByActionType)
{
    NSDictionary *rules = @{
        @"Test Ruleset": @[
            @{
                @"id": @1,
                @"action": @{ @"type": @"upgradeScheme" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{ @"type": @"allowAllRequests" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"main_frame" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{ @"type": @"allow" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{ @"type": @"block" },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
            @{
                @"id": @1,
                @"action": @{
                    @"type": @"modifyHeaders",
                    @"requestHeaders": @[
                        @{ @"header": @"DNT", @"operation": @"set", @"value": @"1" },
                    ],
                },
                @"condition": @{
                    @"regexFilter": @"bananas.com",
                    @"resourceTypes": @[ @"script" ],
                },
            },
        ]
    };

    NSArray *sortedTranslatedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:rules errorStrings:nil];
    EXPECT_NOT_NULL(sortedTranslatedRules);

    EXPECT_NS_EQUAL(sortedTranslatedRules[0][@"action"][@"type"], @"ignore-following-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[1][@"action"][@"type"], @"ignore-following-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[2][@"action"][@"type"], @"block");
    EXPECT_NS_EQUAL(sortedTranslatedRules[3][@"action"][@"type"], @"make-https");
    EXPECT_NS_EQUAL(sortedTranslatedRules[4][@"action"][@"type"], @"ignore-following-rules");
    EXPECT_NS_EQUAL(sortedTranslatedRules[5][@"action"][@"type"], @"modify-headers");
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, RemoveAllContentRuleListsDoesNotRemoveWebExtensionRuleLists)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<iframe src='/frame.html'></iframe>"_s } },
        { "/frame.html"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='background-color: blue'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Remove RuleLists and Load Tab')"
    ]);

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"blockFrame",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    };

    auto *rules = @"[ { \"id\" : 1, \"priority\": 1, \"action\" : { \"type\" : \"block\" }, \"condition\" : { \"urlFilter\" : \"frame\" } } ]";

    auto manager = Util::loadExtension(declarativeNetRequestManifest, @{ @"background.js": backgroundScript, @"rules.json": rules });

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionDeclarativeNetRequest];

    [manager runUntilTestMessage:@"Remove RuleLists and Load Tab"];

    [manager.get().defaultTab.webView.configuration.userContentController removeAllContentRuleLists];

    auto webView = manager.get().defaultTab.webView;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block bool receivedActionNotification { false };
    navigationDelegate.get().contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        receivedActionNotification = true;
    };

    webView.navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    Util::run(&receivedActionNotification);
}

TEST(WKWebExtensionAPIDeclarativeNetRequest, MigrateDeclarativeNetRequestDataToNewFormat)
{
    auto *backgroundScript = Util::constructScript(@[
        @"var expectedResults = [{",
        @"  'id': 1,",
        @"  'condition': {",
        @"      'urlFilter': 'blocksub'",
        @"  },",
        @"  'action': {",
        @"      'type': 'block'",
        @"  }",
        @"}]",

        @"var results",
        @"results = await browser.declarativeNetRequest.getDynamicRules()",

        @"browser.test.assertDeepEq(results, expectedResults)",

        @"browser.test.notifyPass()",
    ]);

    static auto *resources = @{
        @"background.js": backgroundScript,
    };

    auto *declarativeNetRequestManifest = @{
        @"manifest_version": @3,
        @"permissions": @[ @"declarativeNetRequest" ],
        @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
    };

    auto manager = Util::parseExtension(declarativeNetRequestManifest, resources, WKWebExtensionControllerConfiguration._temporaryConfiguration);

    // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
    manager.get().context.uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    [manager load];

    auto *storageDirectory = manager.get().controller.configuration._storageDirectoryPath;
    storageDirectory = [storageDirectory stringByAppendingPathComponent:manager.get().context.uniqueIdentifier];

    static auto *files = @[
        [NSBundle.test_resourcesBundle URLForResource:@"DeclarativeNetRequestRules" withExtension:@"db"],
        [NSBundle.test_resourcesBundle URLForResource:@"DeclarativeNetRequestRules" withExtension:@"db-shm"],
        [NSBundle.test_resourcesBundle URLForResource:@"DeclarativeNetRequestRules" withExtension:@"db-wal"]
    ];

    for (NSURL *file in files) {
        NSString *combinedPath = [storageDirectory stringByAppendingPathComponent:[file lastPathComponent]];
        [NSFileManager.defaultManager copyItemAtURL:file toURL:[NSURL fileURLWithPath:combinedPath] error:nil];
    }

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
