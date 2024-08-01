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
#import "WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>

namespace TestWebKitAPI {

static constexpr auto *sidebarActionManifest = @{
    @"manifest_version": @3,
    @"name": @"SidebarAction Test",
    @"description": @"SidebarAction Test",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"sidebar_action": @{
        @"default_title": @"Test Sidebar",
        @"default_panel": @"sidebar.html",
        @"default_icon": @{
            @"16": @"sidebar-16.png",
            @"32": @"sidebar-32.png",
        },
        @"open_at_install": @NO,
    },
};


static auto *sidePanelManifest = @{
    @"manifest_version": @3,

    @"name": @"SidePanel Test",
    @"description": @"SidePanel Test",
    @"version": @"1",

    @"permissions": @[ @"sidePanel" ],
    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

static auto *sidebarActionAndPanelManifest = @{
    @"manifest_version": @3,

    @"name": @"Sidebar Test (both)",
    @"description": @"Sidebar Test (both)",
    @"version": @"1",

    @"permissions": @[ @"sidePanel" ],

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"sidebar_action": @{
        @"default_title": @"Test Sidebar",
        @"default_panel": @"sidebar.html",
        @"default_icon": @{
            @"16": @"sidebar-16.png",
            @"32": @"sidebar-32.png",
        },
        @"open_at_install": @NO,
    },
};

static auto *neitherSidebarActionNorPanelManifest = @{
    @"manifest_version": @3,
    @"name": @"Sidebar Test (neither)",
    @"description": @"Sidebar Test (neither)",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

// This test fixture allows us to use sidebarConfig (which enables the sidebar feature flag) without manually constructing one on each run
class WKWebExtensionAPISidebar : public testing::Test {
protected:
    WKWebExtensionAPISidebar()
    {
        sidebarConfig = WKWebExtensionControllerConfiguration.defaultConfiguration;
        if (!sidebarConfig.webViewConfiguration)
            sidebarConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"WebExtensionSidebarEnabled"])
                [sidebarConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }

    WKWebExtensionControllerConfiguration *sidebarConfig;
};

TEST_F(WKWebExtensionAPISidebar, APISUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.sidebarAction, undefined)",
        @"browser.test.assertDeepEq(browser.sidePanel, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(neitherSidebarActionNorPanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIAvailableWhenManifestRequests)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.sidebarAction === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.close === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.getPanel === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.getTitle === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.isOpen === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.open === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.setIcon === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.setPanel === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.setTitle === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.toggle === undefined)",

        @"browser.test.assertDeepEq(browser.sidePanel, undefined)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidebarActionManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPIAvailableWhenManifestRequests)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.sidePanel === undefined)",
        @"browser.test.assertFalse(browser.sidePanel?.open === undefined)",
        @"browser.test.assertFalse(browser.sidePanel?.getOptions === undefined)",
        @"browser.test.assertFalse(browser.sidePanel?.setOptions === undefined)",
        @"browser.test.assertFalse(browser.sidePanel?.getPanelBehavior === undefined)",
        @"browser.test.assertFalse(browser.sidePanel?.setPanelBehavior === undefined)",

        @"browser.test.assertDeepEq(browser.sidebarAction, undefined)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidePanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIAvailableWhenManifestRequestsBoth)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.sidebarAction === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.close === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.getPanel === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.getTitle === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.isOpen === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.open === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.setIcon === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.setPanel === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.setTitle === undefined)",
        @"browser.test.assertFalse(browser.sidebarAction.toggle === undefined)",

        @"browser.test.assertDeepEq(browser.sidePanel, undefined)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidebarActionAndPanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIDisallowsMissingArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.sidebarAction.getTitle())",
        @"browser.test.assertThrows(() => browser.sidebarAction.setTitle())",
        @"browser.test.assertThrows(() => browser.sidebarAction.getPanel())",
        @"browser.test.assertThrows(() => browser.sidebarAction.setPanel())",
        @"browser.test.assertThrows(() => browser.sidebarAction.isOpen())",
        @"browser.test.assertThrows(() => browser.sidebarAction.setIcon())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidebarActionManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPIDisallowsMissingArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.sidePanel.getOptions())",
        @"browser.test.assertThrows(() => browser.sidePanel.setOptions())",
        @"browser.test.assertThrows(() => browser.sidePanel.setPanelBehavior())",
        @"browser.test.assertThrows(() => browser.sidePanel.open())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidePanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
