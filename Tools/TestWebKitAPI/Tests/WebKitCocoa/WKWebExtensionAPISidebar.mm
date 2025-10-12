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
#import <WebKit/_WKWebExtensionSidebar.h>

namespace TestWebKitAPI {

#pragma mark - Constants

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

    @"side_panel": @{
        @"default_path": @"sidebar.html",
    },

    @"action": @{
        @"default_title": @"Test Action",
        @"default_popup": @"sidebar.html",
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

#pragma mark - Test Fixture

// This test fixture allows us to use sidebarConfig (which enables the sidebar feature flag) without manually constructing one on each run
class WKWebExtensionAPISidebar : public testing::Test {
protected:
    WKWebExtensionAPISidebar()
    {
        sidebarConfig = WKWebExtensionControllerConfiguration.nonPersistantConfiguration;
        if (!sidebarConfig.webViewConfiguration)
            sidebarConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"WebExtensionSidebarEnabled"])
                [sidebarConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, sidebarConfig);
    }

    WKWebExtensionControllerConfiguration *sidebarConfig;
    RetainPtr<WKWebExtension> extension;
};

#pragma mark - Common Tests

TEST_F(WKWebExtensionAPISidebar, APISUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.sidebarAction, undefined)",
        @"browser.test.assertDeepEq(browser.sidePanel, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(neitherSidebarActionNorPanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

#pragma mark - SidebarAction Tests

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

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIGlobalTitlePersists)
{
    auto *script = @[
        @"await browser.sidebarAction.setTitle({ title: 'Here is a title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Here is a title')",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidebarActionManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIWindowSpecificTitlePersists)
{
    auto *script = @[
        @"let windows = await browser.windows.getAll()",
        @"let [window1, window2] = windows",

        @"await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window-specific title' })",
        @"await browser.sidebarAction.setTitle({ title: 'global title' })",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window-specific title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPITabSpecificTitlePersists)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'tab-specific title' })",
        @"await browser.sidebarAction.setTitle({ title: 'global title' })",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'tab-specific title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab2.id }), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIMixedSpecificityTitlesNotConfused)
{
    auto *script = @[
        @"let allWindows = await browser.windows.getAll()",
        @"let [window1, window2] = allWindows",
        @"let window1Tabs = await browser.tabs.query({ windowId: window1.id })",
        @"let window2Tabs = await browser.tabs.query({ windowId: window2.id })",

        @"await browser.sidebarAction.setTitle({ title: 'global title' })",
        @"await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window specific title' })",
        @"await browser.sidebarAction.setTitle({ tabId: window1Tabs[0].id, title: 'window 1 tab specific title' })",
        @"await browser.sidebarAction.setTitle({ tabId: window2Tabs[0].id, title: 'window 2 tab specific title' })",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window1Tabs[0].id }), 'window 1 tab specific title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window1Tabs[1].id }), 'window specific title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window specific title')",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window2Tabs[0].id }), 'window 2 tab specific title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: window2Tabs[1].id }), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager.get().defaultWindow openNewTab];

    auto window2 = [manager openNewWindow];
    [window2 openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPIWindowSpecificPanelPersists)
{
    auto *script = @[
        @"let windows = await browser.windows.getAll()",
        @"let [window1, window2] = windows",

        @"await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-1.html' })",
        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-2.html' })",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-1.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-2.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), '/sidebar-2.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionAPITabSpecificDocumentPersists)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-1.html' })",
        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-2.html' })",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-1.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-2.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab2.id }), '/sidebar-2.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionDocumentsMixedSpecificityNotConfused)
{
    auto *script = @[
        @"let allWindows = await browser.windows.getAll()",
        @"let [window1, window2] = allWindows",
        @"let window1Tabs = await browser.tabs.query({ windowId: window1.id })",
        @"let window2Tabs = await browser.tabs.query({ windowId: window2.id })",

        @"await browser.sidebarAction.setPanel({ panel: '/global-sidebar.html' })",
        @"await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/window-1-window-sidebar.html' })",
        @"await browser.sidebarAction.setPanel({ tabId: window1Tabs[0].id, panel: '/window-1-tab-sidebar.html' })",
        @"await browser.sidebarAction.setPanel({ tabId: window2Tabs[0].id, panel: '/window-2-tab-sidebar.html' })",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window1Tabs[0].id }), '/window-1-tab-sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window1Tabs[1].id }), '/window-1-window-sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/window-1-window-sidebar.html')",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window2Tabs[0].id }), '/window-2-tab-sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: window2Tabs[1].id }), '/global-sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), '/global-sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/global-sidebar.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"window1-window-sidebar.html" : @"<h1>Window 1 Window Sidebar</h1>",
        @"window1-tab-sidebar.html" : @"<h1>Window 1 Tab Sidebar</h1>",
        @"window2-tab-sidebar.html" : @"<h1>Window 2 Tab Sidebar</h1>",
        @"global-sidebar.html" : @"<h1>Global Sidebar</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager.get().defaultWindow openNewTab];

    auto window2 = [manager openNewWindow];
    [window2 openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionChangeGlobalTitle)
{
    auto *script = @[
        @"await browser.sidebarAction.setTitle({ title: 'Here is a title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Here is a title')",
        @"await browser.sidebarAction.setTitle({ title: 'Here is a different title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Here is a different title')",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidebarActionManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionChangeWindowTitle)
{
    auto *script = @[
        @"let windows = await browser.windows.getAll()",
        @"let [window1, window2] = windows",

        @"await browser.sidebarAction.setTitle({ title: 'global title' })",
        @"await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window-specific title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window-specific title')",

        @"await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'another window-specific title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'another window-specific title')",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionChangeTabTitle)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'tab-specific title' })",
        @"await browser.sidebarAction.setTitle({ title: 'global title' })",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'tab-specific title')",
        @"await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'new tab-specific title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'new tab-specific title')",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab2.id }), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionChangeGlobalPanel)
{
    auto *script = @[
        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-1.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-1.html')",
        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-2.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-2.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionChangeWindowPanel)
{
    auto *script = @[
        @"let windows = await browser.windows.getAll()",
        @"let [window1, window2] = windows",

        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-global.html' })",

        @"await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-1.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-1.html')",
        @"await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-2.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-2.html')",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-global.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), '/sidebar-global.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-global.html" : @"<h1>Global Sidebar</h1>",
        @"sidebar-1.html" : @"<h1>Sidebar 2</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionChangeTabPanel)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-global.html' })",

        @"await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-1.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-1.html')",
        @"await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-2.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-2.html')",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-global.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab2.id }), '/sidebar-global.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-global.html" : @"<h1>Global Sidebar</h1>",
        @"sidebar-1.html" : @"<h1>Sidebar 2</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionClearGlobalTitle)
{
    auto *script = @[
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Test Sidebar')",

        @"await browser.sidebarAction.setTitle({ title: 'A new global title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'A new global title')",

        @"await browser.sidebarAction.setTitle({ title: null })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'Test Sidebar')",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidebarActionManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionClearWindowTitle)
{
    auto *script = @[
        @"let windows = await browser.windows.getAll()",
        @"let [window1, window2] = windows",

        @"await browser.sidebarAction.setTitle({ title: 'global title' })",

        @"await browser.sidebarAction.setTitle({ windowId: window1.id, title: 'window-specific title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'window-specific title')",

        @"await browser.sidebarAction.setTitle({ windowId: window1.id, title: null })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window1.id }), 'global title')",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ windowId: window2.id }), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionClearTabTitle)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidebarAction.setTitle({ tabId: tab1.id, title: 'tab-specific title' })",
        @"await browser.sidebarAction.setTitle({ title: 'global title' })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'tab-specific title')",

        @"await browser.sidebarAction.setTitle({ tabId: tab1.id, title: null })",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab1.id }), 'global title')",

        @"browser.test.assertEq(await browser.sidebarAction.getTitle({}), 'global title')",
        @"browser.test.assertEq(await browser.sidebarAction.getTitle({ tabId: tab2.id }), 'global title')",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidebarActionManifest);

    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionClearGlobalPanel)
{
    auto *script = @[
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')",

        @"await browser.sidebarAction.setPanel({ panel: '/sidebar-1.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), '/sidebar-1.html')",

        @"await browser.sidebarAction.setPanel({ panel: null })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionClearWindowPanel)
{
    auto *script = @[
        @"let windows = await browser.windows.getAll()",
        @"let [window1, window2] = windows",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')",

        @"await browser.sidebarAction.setPanel({ windowId: window1.id, panel: '/sidebar-1.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), '/sidebar-1.html')",
        @"await browser.sidebarAction.setPanel({ windowId: window1.id, panel: null })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window1.id }), 'sidebar.html')",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ windowId: window2.id }), 'sidebar.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);

    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionClearTabPanel)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')",

        @"await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: '/sidebar-1.html' })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), '/sidebar-1.html')",
        @"await browser.sidebarAction.setPanel({ tabId: tab1.id, panel: null })",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab1.id }), 'sidebar.html')",

        @"browser.test.assertEq(await browser.sidebarAction.getPanel({}), 'sidebar.html')",
        @"browser.test.assertEq(await browser.sidebarAction.getPanel({ tabId: tab2.id }), 'sidebar.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
    };

    auto manager = getManagerFor(resources, sidebarActionManifest);
    [manager openNewWindow];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionOpenFailsWithoutUserGesture)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.sidebarAction.open())",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;

    auto manager = getManagerFor(resources, sidebarActionManifest);

    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *) {
        (*presentSidebarCallCountPtr)++;
    };

    [manager loadAndRun];

    EXPECT_EQ(presentSidebarCallCount, 0);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionCloseFailsWithoutUserGesture)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.sidebarAction.close())",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int closeSidebarCallCount = 0;
    int *closeSidebarCallCountPtr = &closeSidebarCallCount;

    auto manager = getManagerFor(resources, sidebarActionManifest);

    manager.get().internalDelegate.closeSidebar = ^(_WKWebExtensionSidebar *) {
        (*closeSidebarCallCountPtr)++;
    };

    [manager loadAndRun];

    EXPECT_EQ(closeSidebarCallCount, 0);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionToggleFailsWithoutUserGesture)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.sidebarAction.toggle())",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int openSidebarCallCount = 0;
    int *openSidebarCallCountPtr = &openSidebarCallCount;
    int closeSidebarCallCount = 0;
    int *closeSidebarCallCountPtr = &closeSidebarCallCount;

    auto manager = getManagerFor(resources, sidebarActionManifest);

    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *) {
        (*openSidebarCallCountPtr)++;
    };

    manager.get().internalDelegate.closeSidebar = ^(_WKWebExtensionSidebar *) {
        (*closeSidebarCallCountPtr)++;
    };

    [manager loadAndRun];

    EXPECT_EQ(openSidebarCallCount, 0);
    EXPECT_EQ(closeSidebarCallCount, 0);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionOpenSucceedsWithUserGesture)
{
    auto *script = @[
        @"await browser.test.runWithUserGesture(() => {",
        @"  return browser.test.assertSafeResolve(() => browser.sidebarAction.open())",
        @"})",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;

    auto manager = getManagerFor(resources, sidebarActionManifest);
    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *sidebar) {
        (*presentSidebarCallCountPtr)++;
        EXPECT_NOT_NULL(sidebar.associatedTab);
        EXPECT_NOT_NULL(sidebar.webExtensionContext);
        EXPECT_NOT_NULL(sidebar.title);
        EXPECT_NOT_NULL(sidebar.webView);
        EXPECT_NOT_NULL(sidebar.viewController);

        NSURL *webViewURL = sidebar.webView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/sidebar.html");
    };

    [manager loadAndRun];

    EXPECT_EQ(presentSidebarCallCount, 1);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionCloseSucceedsWithUserGesture)
{
    auto *script = @[
        @"await browser.test.runWithUserGesture(() => {",
        @"  return browser.test.assertSafeResolve(() => browser.sidebarAction.close())",
        @"})",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int closeSidebarCallCount = 0;
    int *closeSidebarCallCountPtr = &closeSidebarCallCount;

    auto manager = getManagerFor(resources, sidebarActionManifest);
    manager.get().internalDelegate.closeSidebar = ^(_WKWebExtensionSidebar *sidebar) {
        (*closeSidebarCallCountPtr)++;

        // Make sure all the properties are still there when delegate closeSidebar is called, since the sidebar is still displayed at the moment of the call
        EXPECT_NOT_NULL(sidebar.associatedTab);
        EXPECT_NOT_NULL(sidebar.webExtensionContext);
        EXPECT_NOT_NULL(sidebar.title);
        EXPECT_NOT_NULL(sidebar.webView);
        EXPECT_NOT_NULL(sidebar.viewController);

        NSURL *webViewURL = sidebar.webView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/sidebar.html");
    };

    [manager loadAndRun];

    EXPECT_EQ(closeSidebarCallCount, 1);
}

TEST_F(WKWebExtensionAPISidebar, SidebarActionToggleSucceedsWithUserGesture)
{
    auto *script = @[
        @"await browser.test.runWithUserGesture(() => {",
        @"  return browser.test.assertSafeResolve(() => browser.sidebarAction.toggle())",
        @"})",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int openSidebarCallCount = 0;
    int *openSidebarCallCountPtr = &openSidebarCallCount;

    auto manager = getManagerFor(resources, sidebarActionManifest);
    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *) {
        (*openSidebarCallCountPtr)++;
    };

    [manager loadAndRun];

    EXPECT_EQ(openSidebarCallCount, 1);
}

#pragma mark - SidePanel Tests

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

TEST_F(WKWebExtensionAPISidebar, SidePanelAPIGlobalPathPersists)
{
    auto *script = @[
        @"await browser.sidePanel.setOptions({ path: '/sidebar-1.html' })",
        @"let options = await browser.sidePanel.getOptions({})",
        @"browser.test.assertEq(options.path, '/sidebar-1.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar-1.html": @"<h1>Sidebar 1</h1>",
    };

    Util::loadAndRunExtension(sidePanelManifest, resources, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPITabPathPersists)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidePanel.setOptions({ tabId: tab1.id, path: '/sidebar-1.html' })",
        @"await browser.sidePanel.setOptions({ path: '/sidebar-global.html' })",

        @"let tabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })",
        @"let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })",
        @"let globalOptions = await browser.sidePanel.getOptions({})",

        @"browser.test.assertEq(tabOptions.path, '/sidebar-1.html')",
        @"browser.test.assertEq(otherTabOptions.path, '/sidebar-global.html')",
        @"browser.test.assertEq(globalOptions.path, '/sidebar-global.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar-global.html": @"<h1>Global Sidebar</h1>",
        @"sidebar-1.html": @"<h1>Sidebar 1</h1>",
    };

    auto manager = getManagerFor(resources, sidePanelManifest);
    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPIGlobalEnablePersists)
{
    auto *script = @[
        @"let startingOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertTrue(startingOptions.enabled)",

        @"await browser.sidePanel.setOptions({ enabled: false })",
        @"let options = await browser.sidePanel.getOptions({})",
        @"browser.test.assertFalse(options.enabled)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidePanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPITabEnablePersists)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",
        @"let startingOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertTrue(startingOptions.enabled)",

        @"await browser.sidePanel.setOptions({ tabId: tab1.id, enabled: false })",

        @"let tabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })",
        @"let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })",
        @"let globalOptions = await browser.sidePanel.getOptions({})",

        @"browser.test.assertFalse(tabOptions.enabled)",
        @"browser.test.assertTrue(otherTabOptions.enabled)",
        @"browser.test.assertTrue(globalOptions.enabled)",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidePanelManifest);

    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPIModifyGlobalPath)
{
    auto *script = @[
        @"await browser.sidePanel.setOptions({ path: '/sidebar-1.html' })",
        @"let preModOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertEq(preModOptions.path, '/sidebar-1.html')",

        @"await browser.sidePanel.setOptions({ path: '/sidebar-2.html' })",
        @"let postModOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertEq(postModOptions.path, '/sidebar-2.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    Util::loadAndRunExtension(sidePanelManifest, resources, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelAPIModifyTabPath)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidePanel.setOptions({ tabId: tab1.id, path: '/sidebar-1.html' })",
        @"await browser.sidePanel.setOptions({ path: '/sidebar-global.html' })",
        @"let preModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })",
        @"browser.test.assertEq(preModTabOptions.path, '/sidebar-1.html')",

        @"await browser.sidePanel.setOptions({ tabId: tab1.id, path: '/sidebar-2.html' })",
        @"let postModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })",
        @"browser.test.assertEq(postModTabOptions.path, '/sidebar-2.html')",

        @"let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })",
        @"let globalOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertEq(otherTabOptions.path, '/sidebar-global.html')",
        @"browser.test.assertEq(globalOptions.path, '/sidebar-global.html')",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js"  : Util::constructScript(script),
        @"sidebar-global.html" : @"<h1>Global Sidebar</h1>",
        @"sidebar-1.html" : @"<h1>Sidebar 1</h1>",
        @"sidebar-2.html" : @"<h1>Sidebar 2</h1>",
    };

    auto manager = getManagerFor(resources, sidePanelManifest);

    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidePanelModifyGlobalEnable)
{
    auto *script = @[
        @"let startingOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertTrue(startingOptions.enabled)",

        @"await browser.sidePanel.setOptions({ enabled: false })",
        @"let preModOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertFalse(preModOptions.enabled)",

        @"await browser.sidePanel.setOptions({ enabled: true })",
        @"let postModOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertTrue(postModOptions.enabled)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidePanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelModifyTabEnable)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"let [tab1, tab2] = tabs",

        @"await browser.sidePanel.setOptions({ enabled: true })",
        @"await browser.sidePanel.setOptions({ tabId: tab1.id, enabled: true })",
        @"let preModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })",
        @"browser.test.assertTrue(preModTabOptions.enabled)",

        @"await browser.sidePanel.setOptions({ tabId: tab1.id, enabled: false })",
        @"let postModTabOptions = await browser.sidePanel.getOptions({ tabId: tab1.id })",
        @"browser.test.assertFalse(postModTabOptions.enabled)",

        @"let otherTabOptions = await browser.sidePanel.getOptions({ tabId: tab2.id })",
        @"let globalOptions = await browser.sidePanel.getOptions({})",
        @"browser.test.assertTrue(otherTabOptions.enabled)",
        @"browser.test.assertTrue(globalOptions.enabled)",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, sidePanelManifest);

    [manager.get().defaultWindow openNewTab];

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPISidebar, SidePanelOpenForTabFailsWithoutUserGesture)
{
    auto *script = @[
        @"let tabs = await browser.tabs.query({})",
        @"await browser.sidePanel.open({ tabId: tabs[0].id })",
        @"    .then(() => browser.test.notifyFail('sidePanel.open() did not throw without user gesture'))",
        @"    .catch(() => browser.test.notifyPass())",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;

    auto manager = getManagerFor(resources, sidePanelManifest);

    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *) {
        (*presentSidebarCallCountPtr)++;
    };

    [manager loadAndRun];

    EXPECT_EQ(presentSidebarCallCount, 0);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelOpenForWindowFailsWithoutUserGesture)
{
    auto *script = @[
        @"let window = await browser.windows.getCurrent()",
        @"await browser.sidePanel.open({ windowId: window.id })",
        @"    .then(() => browser.test.notifyFail('sidePanel.open() did not throw without user gesture'))",
        @"    .catch(() => browser.test.notifyPass())",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;

    auto manager = getManagerFor(resources, sidePanelManifest);

    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *) {
        (*presentSidebarCallCountPtr)++;
    };

    [manager loadAndRun];

    EXPECT_EQ(presentSidebarCallCount, 0);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelOpenForTabSucceedsWithUserGesture)
{
    auto *script = @[
        @"var tabs = await browser.tabs.query({})",

        @"await browser.test.runWithUserGesture(() => {",
        @"  return browser.test.assertSafeResolve(() => browser.sidePanel.open({ tabId: tabs[0].id }))",
        @"})",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;

    auto manager = getManagerFor(resources, sidePanelManifest);
    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *sidebar) {
        (*presentSidebarCallCountPtr)++;
        EXPECT_NOT_NULL(sidebar.associatedTab);
        EXPECT_NOT_NULL(sidebar.webExtensionContext);
        EXPECT_NOT_NULL(sidebar.title);
        EXPECT_NOT_NULL(sidebar.webView);
        EXPECT_NOT_NULL(sidebar.viewController);

        NSURL *webViewURL = sidebar.webView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/sidebar.html");
    };

    [manager loadAndRun];

    EXPECT_EQ(presentSidebarCallCount, 1);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelOpenForWindowSucceedsWithUserGesture)
{
    auto *script = @[
        @"var currentWindow = await browser.windows.getCurrent()",

        @"await browser.test.runWithUserGesture(() => {",
        @"  return browser.test.assertSafeResolve(() => browser.sidePanel.open({ windowId: currentWindow.id }))",
        @"})",

        @"browser.test.notifyPass()",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;

    auto manager = getManagerFor(resources, sidePanelManifest);
    auto *defaultWindow = [manager defaultWindow];
    auto *newWindow = [manager openNewWindow];
    [manager focusWindow:defaultWindow];

    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *sidebar) {
        (*presentSidebarCallCountPtr)++;
        EXPECT_NOT_NULL(sidebar.associatedTab);
        EXPECT_NOT_NULL(sidebar.webExtensionContext);
        EXPECT_NOT_NULL(sidebar.title);
        EXPECT_NOT_NULL(sidebar.webView);
        EXPECT_NOT_NULL(sidebar.viewController);

        NSURL *webViewURL = sidebar.webView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/sidebar.html");

        EXPECT_FALSE([newWindow.tabs containsObject:sidebar.associatedTab]);
        EXPECT_TRUE([defaultWindow.tabs containsObject:sidebar.associatedTab]);
    };

    [manager loadAndRun];

    EXPECT_EQ(presentSidebarCallCount, 1);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelSetPanelBehaviorPersists)
{
    auto *script = @[
        @"let panelBehavior = await browser.sidePanel.getPanelBehavior()",
        @"browser.test.assertFalse(panelBehavior.openPanelOnActionClick)",

        @"await browser.test.assertSafeResolve(() => browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: true }))",
        @"let newPanelBehavior = await browser.sidePanel.getPanelBehavior()",
        @"browser.test.assertTrue(newPanelBehavior.openPanelOnActionClick)",

        @"await browser.test.assertSafeResolve(() => browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: false }))",
        @"let newNewPanelBehavior = await browser.sidePanel.getPanelBehavior()",
        @"browser.test.assertFalse(newNewPanelBehavior.openPanelOnActionClick)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(sidePanelManifest, @{ @"background.js": Util::constructScript(script) }, sidebarConfig);
}

TEST_F(WKWebExtensionAPISidebar, SidePanelOpensSidebarOnActionClickWhenConfigured)
{
    auto *script = @[
        @"await browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: true })",
        @"let initialPanelBehavior = await browser.sidePanel.getPanelBehavior()",
        @"browser.test.assertTrue(initialPanelBehavior.openPanelOnActionClick)",

        @"browser.test.sendMessage('Perform action')",

        @"await browser.sidePanel.setPanelBehavior({ openPanelOnActionClick: false })",
        @"let newPanelBehavior = await browser.sidePanel.getPanelBehavior()",
        @"browser.test.assertFalse(newPanelBehavior.openPanelOnActionClick)",

        @"browser.test.sendMessage('Perform action again')",
    ];

    auto *resources = @{
        @"background.js": Util::constructScript(script),
        @"sidebar.html": @"<h1>Sidebar</h1>",
    };

    int presentSidebarCallCount = 0;
    int *presentSidebarCallCountPtr = &presentSidebarCallCount;
    int presentActionPopupCallCount = 0;
    int *presentActionPopupCallCountPtr = &presentActionPopupCallCount;

    auto manager = getManagerFor(resources, sidePanelManifest);
    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        (*presentActionPopupCallCountPtr)++;
        [manager done];
    };
    manager.get().internalDelegate.presentSidebar = ^(_WKWebExtensionSidebar *) {
        (*presentSidebarCallCountPtr)++;
    };

    [manager load];
    [manager runUntilTestMessage:@"Perform action"];

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager runUntilTestMessage:@"Perform action again"];

    EXPECT_EQ(presentSidebarCallCount, 1);
    EXPECT_EQ(presentActionPopupCallCount, 0);

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager run];

    EXPECT_EQ(presentSidebarCallCount, 1);
    EXPECT_EQ(presentActionPopupCallCount, 1);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
