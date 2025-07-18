/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "HTTPServer.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionPermissionPrivate.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>

namespace TestWebKitAPI {

#pragma mark - Constants

static constexpr auto *bookmarkOffManifest = @{
    @"manifest_version": @3,
    @"name": @"bookmarkpermission off Test",
    @"description": @"bookmarkpermission off Test",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

static auto *bookmarkOnManifest = @{
    @"manifest_version": @3,
    @"name": @"bookmarkpermission on Test",
    @"description": @"bookmarkpermission on Test",
    @"version": @"1",

    @"permissions": @[ @"bookmarks" ],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
    @"action": @{
        @"default_title": @"Test Action",
        @"default_popup": @"popup.html",
    },
};

#pragma mark - Test Fixture

class WKWebExtensionAPIBookmarks : public testing::Test {
protected:
    WKWebExtensionAPIBookmarks()
    {
        bookmarkConfig = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
        if (!bookmarkConfig.webViewConfiguration)
            bookmarkConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"WebExtensionBookmarksEnabled"])
                [bookmarkConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }
    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, bookmarkConfig);
    }

    WKWebExtensionControllerConfiguration *bookmarkConfig;
};

#pragma mark - Common Tests

TEST_F(WKWebExtensionAPIBookmarks, APISUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.bookmarks, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOffManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

#pragma mark - more Tests

TEST_F(WKWebExtensionAPIBookmarks, APIAvailableWhenManifestRequests)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.bookmarks === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.create === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getChildren === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getRecent === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getSubTree === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getTree === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.get === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.move === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.remove === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.removeTree === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.search === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.update === undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIDisallowsMissingArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.create())",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren())",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent())",
        @"browser.test.assertThrows(() => browser.bookmarks.getSubTree())",
        @"browser.test.assertThrows(() => browser.bookmarks.get())",
        @"browser.test.assertThrows(() => browser.bookmarks.move())",
        @"browser.test.assertThrows(() => browser.bookmarks.remove())",
        @"browser.test.assertThrows(() => browser.bookmarks.removeTree())",
        @"browser.test.assertThrows(() => browser.bookmarks.search())",
        @"browser.test.assertThrows(() => browser.bookmarks.update())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIDisallowedIncorrectArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren(123), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren({}), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.get('test', 'test'), /The 'callback' value is invalid, because a function is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.move(123, {}), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.move('someId', 'not-an-object'), /The 'destination' value is invalid, because an object is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.remove(123), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.search(123, 'test'), /The 'callback' value is invalid, because a function is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.update(123, {}), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICheckgetRecent)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren(123), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.log('workingtest line1')",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren({}), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.log('workingtest line2')",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent(-1), /The 'numberOfItems' value is invalid, because it must be at least 1./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent(0), /The 'numberOfItems' value is invalid, because it must be at least 1./i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIMockNodeWithgetRecent)
{
    auto *script = @[
        @"browser.bookmarks.create({id: 'id_old', title: 'Oldest Bookmark', url: 'http://example.com/1', dateAdded: 1000})",
        @"browser.bookmarks.create({id: 'id_new', title: 'Newest Bookmark', url: 'http://example.com/3', dateAdded: 3000})",
        @"browser.bookmarks.create({id: 'id_mid', title: 'Middle Bookmark', url: 'http://example.com/2', dateAdded: 2000})",
        @"let recent = await browser.bookmarks.getRecent(2)",
        @"browser.test.assertEq(2, recent.length, 'Should return exactly 2 bookmarks')",
        @"browser.test.assertEq('id_new', recent[0].id, 'First result should be the newest bookmark')",
        @"browser.test.assertEq('id_mid', recent[1].id, 'Second result should be the middle bookmark')",
        @"let recent2 = await browser.bookmarks.getRecent(5)",
        @"browser.test.assertEq(3, recent2.length, 'Should adapt and return the max available which is 3')",
        @"browser.test.assertEq('id_new', recent2[0].id, 'First result should be the newest bookmark')",
        @"browser.test.assertEq('id_mid', recent2[1].id, 'Second result should be the middle bookmark')",
        @"browser.test.assertEq('id_old', recent2[2].id, 'Second result should be the middle bookmark')",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICreateParse)
{
    auto *script = @[
        @"let createdNode = await browser.bookmarks.create({id: 'test1', title: 'My Test Bookmark', url: 'https://example.com/test1'})",
        @"browser.test.assertEq('My Test Bookmark', createdNode.title, 'Title should match');",
        @"browser.test.assertEq('https://example.com/test1', createdNode.url, 'URL should match');",
        @"browser.test.assertEq('bookmark', createdNode.type, 'Type should be bookmark');",
        @"let createdNode2 = await browser.bookmarks.create({id: 'test2', title: 'My Test Folder', parentId: 'testFavorites'})",
        @"browser.test.assertEq('My Test Folder', createdNode2.title, 'Title should match');",
        @"browser.test.assertEq('folder', createdNode2.type, 'type should be folder because url is not specified');",
        @"browser.test.assertEq('testfav', createdNode2.parentId, 'parentId should match');",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetTree)
{
    auto *script = @[
        @"let bookmark1 = await browser.bookmarks.create({type: 'bookmark', id: 'bookmark1', title: 'Top Bookmark 1', url: 'http://example.com/bm1'});",
        @"let folder1 = await browser.bookmarks.create({id: 'folder1', type: 'folder', title: 'Top Folder 1'});",
        @"let bookmark2 = await browser.bookmarks.create({id: 'bookmark2', title: 'Child Bookmark 2', url: 'http://example.com/bm2', parentId: folder1.id});",
        @"let bookmark3 = await browser.bookmarks.create({id: 'bookmark3', title: 'Top Bookmark 3', url: 'http://example.com/bm3'});",
        @"let tree = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(1, tree.length, 'Tree should have one root node');",
        @"let root = tree[0];",
        @"browser.test.assertEq('testBookmarksRoot', root.id, 'Root node ID should be root');",
        @"browser.test.assertEq('folder', root.type, 'Root node type should be folder');",
        @"browser.test.assertTrue(root.children.length >= 1, 'Root should have at least one child (default folder)');",
        @"let foundBookmark1 = root.children.find(n => n.id === bookmark1.id);",
        @"browser.test.assertEq('Top Bookmark 1', foundBookmark1.title, 'Bm1 title matches');",
        @"browser.test.assertEq('http://example.com/bm1', foundBookmark1.url, 'Bm1 URL matches');",
        @"browser.test.assertEq('bookmark', foundBookmark1.type, 'Bm1 type is bookmark');",
        @"browser.test.assertEq(root.id, foundBookmark1.parentId, 'Bm1 parentId matches default folder');",
        @"let foundFolder1 = root.children.find(n => n.id === folder1.id);",
        @"browser.test.assertEq('Top Folder 1', foundFolder1.title, 'Folder1 title matches');",
        @"browser.test.assertEq('folder', foundFolder1.type, 'Folder1 type is folder');",
        @"browser.test.assertEq(root.id, foundFolder1.parentId, 'Folder1 parentId matches default folder');",
        @"browser.test.assertEq(bookmark2.id, foundFolder1.children[0].id, 'Folder1 should have children array');",
        @"let foundBookmark2 = foundFolder1.children.find(n => n.id === bookmark2.id);",
        @"browser.test.assertEq('Child Bookmark 2', foundBookmark2.title, 'Bm2 title matches');",
        @"browser.test.assertEq('http://example.com/bm2', foundBookmark2.url, 'Bm2 URL matches');",
        @"browser.test.assertEq('bookmark', foundBookmark2.type, 'Bm2 type is bookmark');",
        @"browser.test.assertEq(folder1.id, foundBookmark2.parentId, 'Bm2 parentId matches Folder1');",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

}

#endif

