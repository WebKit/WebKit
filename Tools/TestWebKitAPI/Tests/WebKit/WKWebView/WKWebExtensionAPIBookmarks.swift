// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_WK_WEB_EXTENSIONS_BOOKMARKS

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private._WKWebExtension
private import WebKit_Private._WKWebExtensionBookmark
private import WebKit_Private._WKWebExtensionBookmarkConfiguration

import struct Foundation.URL
import struct Swift.String

// MARK: - Test bookmarks

/// Holds the bookmarks that the nodes read and change.
@MainActor
private final class TestBookmarkStore {
    let topLevelBookmarks = NSMutableArray()
    var nextIdentifier = 100

    /// Whether nodes and the delegate call their completion handlers later on the main queue, instead of before returning.
    var completesAsynchronously = false

    /// When set, every change a node is asked to make fails with an error that has this description.
    var failureMessage: String?

    /// Whether creating a bookmark completes with neither a bookmark nor an error.
    var createReturnsNoBookmark = false

    /// Whether nodes answer from a copy of their bookmark while sending title and URL changes to the stored one.
    var nodesAreSnapshots = false

    var failure: NSError? {
        guard let failureMessage else {
            return nil
        }

        return NSError(domain: "TestBookmarks", code: 1, userInfo: [NSLocalizedDescriptionKey: failureMessage])
    }

    private static func siblings(of target: NSMutableDictionary, in bookmarks: NSMutableArray) -> NSMutableArray? {
        if bookmarks.indexOfObjectIdentical(to: target) != NSNotFound {
            return bookmarks
        }

        for case let bookmark as NSDictionary in bookmarks {
            guard let children = bookmark["children"] as? NSMutableArray else {
                continue
            }

            if let found = siblings(of: target, in: children) {
                return found
            }
        }

        return nil
    }

    func siblings(of bookmark: NSMutableDictionary) -> NSMutableArray? {
        Self.siblings(of: bookmark, in: topLevelBookmarks)
    }

    func complete(_ block: @escaping () -> Void) {
        if completesAsynchronously {
            DispatchQueue.main.async {
                block()
            }
            return
        }

        block()
    }
}

/// A bookmark or folder backed by one of the store's dictionaries, whose changes go to the store.
///
/// When the store's `nodesAreSnapshots` is set, the node answers from a copy of the dictionary instead, and its title
/// and URL changes go to the stored dictionary.
@MainActor
private final class TestBookmarkNode: NSObject, _WKWebExtensionBookmark {
    private let dictionary: NSMutableDictionary
    private let originalDictionary: NSMutableDictionary?
    private let store: TestBookmarkStore

    init(dictionary: NSDictionary, store: TestBookmarkStore) {
        var nodeDictionary = dictionary as? NSMutableDictionary ?? NSMutableDictionary(dictionary: dictionary)
        var originalDictionary: NSMutableDictionary?

        // A snapshot node answers from a copy and sends changes to the stored bookmark, like an app whose nodes are values.
        if store.nodesAreSnapshots, let dictionary = dictionary as? NSMutableDictionary {
            originalDictionary = dictionary
            nodeDictionary = NSMutableDictionary(dictionary: dictionary)
        }

        self.dictionary = nodeDictionary
        self.originalDictionary = originalDictionary
        self.store = store
    }

    private var mutableChildren: NSMutableArray {
        if dictionary["children"] == nil {
            dictionary["children"] = NSMutableArray()
        }

        // swift-format-ignore: NeverForceUnwrap
        return dictionary["children"] as! NSMutableArray
    }

    @objc(identifierForWebExtensionContext:)
    func identifier(for context: WKWebExtensionContext) -> String {
        dictionary["id"] as? String ?? ""
    }

    @objc(titleForWebExtensionContext:)
    func title(for context: WKWebExtensionContext) -> String? {
        dictionary["title"] as? String
    }

    @objc(urlForWebExtensionContext:)
    func url(for context: WKWebExtensionContext) -> URL? {
        let url = dictionary["url"] as? String ?? ""
        return url.isEmpty ? nil : URL(string: url)
    }

    @objc(folderTypeForWebExtensionContext:)
    func folderType(for context: WKWebExtensionContext) -> _WKWebExtension.BookmarkFolderType {
        let folderType = dictionary["folderType"] as? String
        if folderType == "bookmarks-bar" {
            return .bookmarksBar
        }

        if folderType == "managed" {
            return .managed
        }

        if folderType == "other" {
            return .other
        }

        return .none
    }

    @objc(isManagedForWebExtensionContext:)
    func isManaged(for context: WKWebExtensionContext) -> Bool {
        (dictionary["managed"] as? NSNumber)?.boolValue ?? false
    }

    @objc(isSyncedForWebExtensionContext:)
    func isSynced(for context: WKWebExtensionContext) -> Bool {
        (dictionary["syncing"] as? NSNumber)?.boolValue ?? false
    }

    @objc(bookmarkTypeForWebExtensionContext:)
    func bookmarkType(for context: WKWebExtensionContext) -> _WKWebExtension.BookmarkType {
        if dictionary["type"] as? String == "folder" {
            return .folder
        }

        let url = dictionary["url"] as? String ?? ""
        return url.isEmpty ? .folder : .bookmark
    }

    @objc(childrenForWebExtensionContext:)
    func children(for context: WKWebExtensionContext) -> [any _WKWebExtensionBookmark]? {
        guard let childDictionaries = dictionary["children"] as? [NSDictionary] else {
            return nil
        }

        var children: [any _WKWebExtensionBookmark] = []
        for childDictionary in childDictionaries {
            children.append(TestBookmarkNode(dictionary: childDictionary, store: store))
        }

        return children
    }

    @objc(dateAddedForWebExtensionContext:)
    func dateAdded(for context: WKWebExtensionContext) -> Date? {
        guard let dateValue = dictionary["dateAdded"] as? NSNumber else {
            return nil
        }

        return Date(timeIntervalSince1970: dateValue.doubleValue / 1000.0)
    }

    @objc(createChildUsingConfiguration:forWebExtensionContext:completionHandler:)
    func createChild(
        using configuration: _WKWebExtension.BookmarkConfiguration,
        for context: WKWebExtensionContext,
        completionHandler: @escaping ((any _WKWebExtensionBookmark)?, (any Error)?) -> Void
    ) {
        if store.failure != nil || store.createReturnsNoBookmark {
            store.complete { [store] in
                completionHandler(nil, store.failure)
            }
            return
        }

        let newDictionary = NSMutableDictionary()
        newDictionary["id"] = String(store.nextIdentifier)
        store.nextIdentifier += 1
        newDictionary["title"] = configuration.title
        newDictionary["dateAdded"] = NSNumber(value: Date().timeIntervalSince1970 * 1000.0)

        if configuration.type == .bookmark {
            newDictionary["url"] = configuration.url?.absoluteString
        } else {
            newDictionary["type"] = "folder"
            newDictionary["children"] = NSMutableArray()
        }

        let children = mutableChildren
        children.insert(newDictionary, at: min(configuration.index, children.count))

        store.complete { [store] in
            completionHandler(TestBookmarkNode(dictionary: newDictionary, store: store), nil)
        }
    }

    @objc(setTitle:forWebExtensionContext:completionHandler:)
    func setTitle(_ title: String, for context: WKWebExtensionContext, completionHandler: @escaping ((any Error)?) -> Void) {
        if store.failure != nil {
            store.complete { [store] in
                completionHandler(store.failure)
            }
            return
        }

        let bookmark = originalDictionary ?? dictionary
        bookmark["title"] = title

        store.complete {
            completionHandler(nil)
        }
    }

    @objc(setURL:forWebExtensionContext:completionHandler:)
    func setURL(_ url: URL, for context: WKWebExtensionContext, completionHandler: @escaping ((any Error)?) -> Void) {
        if store.failure != nil {
            store.complete { [store] in
                completionHandler(store.failure)
            }
            return
        }

        let bookmark = originalDictionary ?? dictionary
        bookmark["url"] = url.absoluteString

        store.complete {
            completionHandler(nil)
        }
    }

    @objc(moveToParent:atIndex:forWebExtensionContext:completionHandler:)
    func move(
        toParent parent: any _WKWebExtensionBookmark,
        index: Int,
        for context: WKWebExtensionContext,
        completionHandler: @escaping ((any Error)?) -> Void
    ) {
        if store.failure != nil {
            store.complete { [store] in
                completionHandler(store.failure)
            }
            return
        }

        // swift-format-ignore: NeverForceUnwrap
        let newParent = parent as! TestBookmarkNode
        let oldSiblings = store.siblings(of: dictionary)
        let newSiblings = newParent.mutableChildren

        // Like the message to nil it replaces, this is 0 when the bookmark isn't in the store.
        let oldIndex = oldSiblings?.indexOfObjectIdentical(to: dictionary) ?? 0
        oldSiblings?.removeObject(at: oldIndex)

        var index = index
        if oldSiblings === newSiblings && oldIndex < index {
            index -= 1
        }

        newSiblings.insert(dictionary, at: min(index, newSiblings.count))

        store.complete {
            completionHandler(nil)
        }
    }

    @objc(removeForWebExtensionContext:completionHandler:)
    func remove(for context: WKWebExtensionContext, completionHandler: @escaping ((any Error)?) -> Void) {
        if store.failure != nil {
            store.complete { [store] in
                completionHandler(store.failure)
            }
            return
        }

        store.siblings(of: dictionary)?.removeObject(identicalTo: dictionary)

        store.complete {
            completionHandler(nil)
        }
    }
}

/// A node that implements only `identifier(for:)`, so WebKit uses its defaults for everything else and reports changes
/// as not implemented.
@MainActor
private final class TestMinimalBookmarkNode: NSObject, _WKWebExtensionBookmark {
    @objc(identifierForWebExtensionContext:)
    func identifier(for context: WKWebExtensionContext) -> String {
        "minimal"
    }
}

/// A bookmark that can be read but not changed.
@MainActor
private final class TestStaticBookmarkNode: NSObject, _WKWebExtensionBookmark {
    @objc(identifierForWebExtensionContext:)
    func identifier(for context: WKWebExtensionContext) -> String {
        "read-only"
    }

    @objc(titleForWebExtensionContext:)
    func title(for context: WKWebExtensionContext) -> String? {
        "WebKit"
    }

    @objc(urlForWebExtensionContext:)
    func url(for context: WKWebExtensionContext) -> URL? {
        URL(string: "https://webkit.org/")
    }
}

@MainActor
struct WKWebExtensionAPIBookmarksTests {
    private let bookmarksNoPermissionManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "Bookmarks No Permission Test",
        "description": "Bookmarks No Permission Test",
        "version": "1",

        "permissions": [],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    private let bookmarksManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "Bookmarks Test",
        "description": "Bookmarks Test",
        "version": "1",

        "permissions": ["bookmarks"],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
        "action": [
            "default_title": "Test Action",
            "default_popup": "popup.html",
        ],
    ]

    // MARK: - Helpers

    /// A controller configuration that turns on the bookmarks feature flag, so that each test doesn't have to make one.
    private let configuration: WKWebExtensionController.Configuration

    /// The bookmarks that `configureBookmarksDelegate(_:)` makes the delegate provide.
    ///
    /// Starts with an empty default folder.
    private let bookmarkStore: TestBookmarkStore

    init() {
        configuration = configurationEnablingFeature("WebExtensionBookmarksEnabled")

        bookmarkStore = TestBookmarkStore()
        bookmarkStore.topLevelBookmarks.add(
            NSMutableDictionary(dictionary: [
                "id": "other", "title": "Other", "type": "folder", "folderType": "other", "children": NSMutableArray(),
            ])
        )
    }

    private func manager(backgroundScript: String, manifest: [String: Any]) -> TestWebExtensionManager {
        manager(resources: ["background.js": backgroundScript], manifest: manifest)
    }

    private func manager(resources: [String: Any], manifest: [String: Any]) -> TestWebExtensionManager {
        parseWebExtension(manifest: manifest, resources: resources, configuration: configuration)
    }

    private func loadAndRun(manifest: [String: Any], resources: [String: Any]) async throws {
        let manager = try loadWebExtension(manifest: manifest, resources: resources, configuration: configuration)
        try await manager.run()
    }

    /// Makes the delegate provide the top-level bookmarks in `bookmarkStore`, as nodes that read and change the store.
    private func configureBookmarksDelegate(_ manager: TestWebExtensionManager) {
        let store = bookmarkStore
        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            store.complete {
                var nodes: [any _WKWebExtensionBookmark] = []
                for case let dictionary as NSDictionary in store.topLevelBookmarks {
                    nodes.append(TestBookmarkNode(dictionary: dictionary, store: store))
                }

                completionHandler(nodes, nil)
            }
        }
    }

    @Test
    func apiUnavailableWhenManifestDoesNotRequest() async throws {
        let backgroundScript = """
            browser.test.assertDeepEq(browser.bookmarks, undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksNoPermissionManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func apiAvailableWhenManifestRequests() async throws {
        let backgroundScript = """
            browser.test.assertFalse(browser.bookmarks === undefined)
            browser.test.assertFalse(browser.bookmarks.create === undefined)
            browser.test.assertFalse(browser.bookmarks.getChildren === undefined)
            browser.test.assertFalse(browser.bookmarks.getRecent === undefined)
            browser.test.assertFalse(browser.bookmarks.getSubTree === undefined)
            browser.test.assertFalse(browser.bookmarks.getTree === undefined)
            browser.test.assertFalse(browser.bookmarks.get === undefined)
            browser.test.assertFalse(browser.bookmarks.move === undefined)
            browser.test.assertFalse(browser.bookmarks.remove === undefined)
            browser.test.assertFalse(browser.bookmarks.removeTree === undefined)
            browser.test.assertFalse(browser.bookmarks.search === undefined)
            browser.test.assertFalse(browser.bookmarks.update === undefined)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func missingArguments() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.bookmarks.create())
            browser.test.assertThrows(() => browser.bookmarks.getChildren())
            browser.test.assertThrows(() => browser.bookmarks.getRecent())
            browser.test.assertThrows(() => browser.bookmarks.getSubTree())
            browser.test.assertThrows(() => browser.bookmarks.get())
            browser.test.assertThrows(() => browser.bookmarks.move())
            browser.test.assertThrows(() => browser.bookmarks.remove())
            browser.test.assertThrows(() => browser.bookmarks.removeTree())
            browser.test.assertThrows(() => browser.bookmarks.search())
            browser.test.assertThrows(() => browser.bookmarks.update())
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func incorrectArguments() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.bookmarks.getChildren(123), /The 'id' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.bookmarks.getChildren({}), /The 'id' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.get('test', 'test'), /The 'callback' value is invalid, because a function is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.move(123, {}), /The 'id' value is invalid, because a string is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.move('someId', 'not-an-object'), /The 'destination' value is invalid, because an object is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.remove(123), /The 'id' value is invalid, because a string is expected/i)
            browser.test.assertThrows(() => browser.bookmarks.search(123, 'test'), /The 'callback' value is invalid, because a function is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.update(123, {}), /The 'id' value is invalid, because a string is expected./i)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func getRecentWithInvalidArguments() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)
            browser.test.assertThrows(() => browser.bookmarks.getRecent(-1), /The 'numberOfItems' value is invalid, because it must be at least 1./i)
            browser.test.assertThrows(() => browser.bookmarks.getRecent(0), /The 'numberOfItems' value is invalid, because it must be at least 1./i)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func getRecent() async throws {
        let backgroundScript = """
            let oldBm = await browser.bookmarks.create({title: 'Oldest Bookmark', url: 'http://example.com/1'})
            let midBm = await browser.bookmarks.create({title: 'Middle Bookmark', url: 'http://example.com/2'})
            let newBm = await browser.bookmarks.create({title: 'Newest Bookmark', url: 'http://example.com/3'})
            let recent = await browser.bookmarks.getRecent(2)
            browser.test.assertEq(2, recent.length, 'Should return exactly 2 bookmarks')
            browser.test.assertEq('Newest Bookmark', recent[0].title, 'First result should be the newest bookmark')
            browser.test.assertEq('Middle Bookmark', recent[1].title, 'Second result should be the middle bookmark')
            let newFolder = await browser.bookmarks.create({title: 'Newest Folder'})
            let recent2 = await browser.bookmarks.getRecent(5)
            browser.test.assertEq(3, recent2.length, 'Should adapt and return the max available which is 3')
            browser.test.assertEq('Newest Bookmark', recent2[0].title, 'First result should be the newest bookmark')
            browser.test.assertEq('Middle Bookmark', recent2[1].title, 'Second result should be the middle bookmark')
            browser.test.assertEq('Oldest Bookmark', recent2[2].title, 'Third result should be the oldest bookmark')
            browser.test.notifyPass()
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func create() async throws {
        let backgroundScript = """
            let createdNode = await browser.bookmarks.create({title: 'My Test Bookmark', url: 'https://example.com/test1'})
            browser.test.assertEq('My Test Bookmark', createdNode.title, 'Title should match');
            browser.test.assertEq('https://example.com/test1', createdNode.url, 'URL should match');
            browser.test.assertEq('bookmark', createdNode.type, 'Type should be bookmark');
            let createdNode2 = await browser.bookmarks.create({title: 'My Test Folder'})
            browser.test.assertEq('My Test Folder', createdNode2.title, 'Title should match');
            browser.test.assertEq('folder', createdNode2.type, 'type should be folder because url is not specified');
            let createdNode3 = await browser.bookmarks.create({title: 'My Children Folder', parentId: createdNode2.id})
            browser.test.assertEq('My Children Folder', createdNode3.title, 'Title should match');
            browser.test.assertEq('folder', createdNode3.type, 'type should be folder because url is not specified');
            browser.test.assertEq(createdNode2.id, createdNode3.parentId, 'parentId should match');
            browser.test.assertEq('other', createdNode.parentId, 'A bookmark without a parent should be in the default folder');
            browser.test.assertEq(0, createdNode.index, 'The first top-level bookmark should have index 0');
            browser.test.assertEq(0, createdNode3.index, 'The first child should have index 0');
            browser.test.notifyPass()
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func getTree() async throws {
        let backgroundScript = """
            let bookmark1 = await browser.bookmarks.create({type: 'bookmark', title: 'Top Bookmark 1', url: 'http://example.com/bm1'});
            let folder1 = await browser.bookmarks.create({type: 'folder', title: 'Top Folder 1'});
            let bookmark2 = await browser.bookmarks.create({title: 'Child Bookmark 2', url: 'http://example.com/bm2', parentId: folder1.id});
            let bookmark3 = await browser.bookmarks.create({title: 'Top Bookmark 3', url: 'http://example.com/bm3'});
            let tree = await browser.bookmarks.getTree();
            browser.test.assertTrue(Array.isArray(tree), 'The tree should be an array');
            browser.test.assertEq(1, tree.length, 'The tree should have one root');
            browser.test.assertEq('0', tree[0].id, 'The root should have the root identifier');
            browser.test.assertEq('', tree[0].title, 'The root should have an empty title');
            browser.test.assertEq('folder', tree[0].type, 'The root should be a folder');
            browser.test.assertTrue(tree[0].parentId === undefined, 'The root should not have a parent');
            let root = tree[0].children;
            browser.test.assertEq(1, root.length, 'The root should have the default folder');
            browser.test.assertEq('other', root[0].folderType, 'The default folder should report its folder type');
            root = root[0].children;
            browser.test.assertEq(3, root.length, 'The default folder should have the three new nodes');
            let foundBookmark1 = root.find(n => n.title === bookmark1.title);
            browser.test.assertEq('Top Bookmark 1', foundBookmark1.title, 'Bm1 title matches');
            browser.test.assertEq('http://example.com/bm1', foundBookmark1.url, 'Bm1 URL matches');
            browser.test.assertEq('bookmark', foundBookmark1.type, 'Bm1 type is bookmark');
            let foundFolder1 = root.find(n => n.title === folder1.title);
            browser.test.assertEq('Top Folder 1', foundFolder1.title, 'Folder1 title matches');
            browser.test.assertEq('folder', foundFolder1.type, 'Folder1 type is folder');
            browser.test.assertEq(bookmark2.title, foundFolder1.children[0].title, 'Folder1 should have children array');
            let foundBookmark2 = foundFolder1.children.find(n => n.title === bookmark2.title);
            browser.test.assertEq('Child Bookmark 2', foundBookmark2.title, 'Bm2 title matches');
            browser.test.assertEq('http://example.com/bm2', foundBookmark2.url, 'Bm2 URL matches');
            browser.test.assertEq('bookmark', foundBookmark2.type, 'Bm2 type is bookmark');
            browser.test.assertEq(folder1.id, foundBookmark2.parentId, 'Bm2 parentId matches Folder1');
            browser.test.assertEq('other', foundBookmark1.parentId, 'Bm1 parentId matches the default folder');
            browser.test.assertEq(0, foundBookmark1.index, 'Bm1 index is its position in the default folder');
            browser.test.notifyPass()
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)
        try await manager.loadAndRun()
    }

    @Test
    func createAndGetTree() async throws {
        let backgroundScript = """
            let folder = await browser.bookmarks.create({ title: 'Test Folder' });
            browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');
            browser.test.assertTrue(!!folder.id, 'Folder should have an ID');
            let bookmark = await browser.bookmarks.create({ parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });
            browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');
            browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');
            let rootNode = (await browser.bookmarks.getTree())[0].children[0].children;
            browser.test.assertTrue(Array.isArray(rootNode), 'Root object should have a children array');
            browser.test.assertEq(rootNode.length, 1);
            browser.test.assertEq(rootNode[0].title, 'Test Folder');
            browser.test.assertTrue(Array.isArray(rootNode[0].children), 'Folder should have a children array');
            browser.test.assertEq(rootNode[0].children[0].title, 'WebKit.org', 'Child bookmark in tree should have correct title');
            browser.test.assertEq(rootNode[0].children[0].url, 'https://webkit.org/', 'Child bookmark in tree should have correct URL');
            let bookmark2 = await browser.bookmarks.create({ id: 'topLevelBookmark', title: 'Test Top Bookmark', url: 'https://coolbook.com/' });
            browser.test.assertEq(bookmark2.title, 'Test Top Bookmark', 'Bookmark title should be correct');
            browser.test.assertEq(bookmark2.url, 'https://coolbook.com/', 'Bookmark URL should be correct');
            let updatedRootNode = (await browser.bookmarks.getTree())[0].children[0].children;
            browser.test.assertEq(updatedRootNode.length, 2);
            browser.test.assertEq(updatedRootNode[0].title, 'Test Folder');
            browser.test.assertEq(updatedRootNode[1].title, 'Test Top Bookmark');
            browser.test.notifyPass();
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)
        try await manager.loadAndRun()
    }

    @Test
    func getSubTreeChildren() async throws {
        let backgroundScript = """
            let folder = await browser.bookmarks.create({title: 'Test Folder' });
            browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');
            browser.test.assertTrue(!!folder.id, 'Folder should have an ID');
            let bookmark = await browser.bookmarks.create({parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });
            browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');
            browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');
            let folder2 = await browser.bookmarks.create({parentId: folder.id, title: 'Folder 2'});
            browser.test.assertEq(folder2.title, 'Folder 2', 'Folder title should be correct');
            browser.test.assertEq(folder2.parentId, folder.id, 'Folder parentId should be correct');
            let bookmark2 = await browser.bookmarks.create({parentId: folder2.id, title: 'Bookmark 2', url: 'https://bookmark2.org/' });
            let subtreeFolder1 = await browser.bookmarks.getSubTree(folder.id);
            browser.test.assertTrue(Array.isArray(subtreeFolder1), 'subtreeFolder1 should be an array');
            browser.test.assertEq(1, subtreeFolder1.length, 'subtreeFolder1 array should only contain the requested node');
            browser.test.assertEq(folder.id, subtreeFolder1[0].id, 'The requested node should be returned');
            browser.test.assertEq(2, subtreeFolder1[0].children.length, 'The requested node should have 2 children');
            browser.test.assertEq(bookmark.id, subtreeFolder1[0].children[0].id, 'First child is bookmark');
            browser.test.assertEq('Folder 2', subtreeFolder1[0].children[1].title, 'Second child title should be Folder 2');
            browser.test.assertEq('folder', subtreeFolder1[0].children[1].type, 'type should be folder');
            browser.test.assertEq(1, subtreeFolder1[0].children[1].children.length, 'Folder 2 should have 1 child');
            browser.test.assertEq(bookmark2.title, subtreeFolder1[0].children[1].children[0].title, 'bookmark2 should be child of folder2');
            let subtreeBookmark = await browser.bookmarks.getSubTree(bookmark.id);
            browser.test.assertTrue(Array.isArray(subtreeBookmark), 'subtreeBookmark should be an array');
            browser.test.assertEq(1, subtreeBookmark.length, 'subtreeBookmark array should have 1 element');
            browser.test.assertEq(bookmark.id, subtreeBookmark[0].id, 'The requested bookmark should be returned');
            browser.test.assertTrue(subtreeBookmark[0].children === undefined, 'A bookmark should not have children');
            let childrenOfBookmark = await browser.bookmarks.getChildren(bookmark.id);
            browser.test.assertTrue(Array.isArray(childrenOfBookmark), 'childrenOfBookmark should be an array');
            browser.test.assertEq(0, childrenOfBookmark.length, 'A bookmark should have no children');
            let subtreeRoot = await browser.bookmarks.getSubTree('0');
            browser.test.assertEq('0', subtreeRoot[0].id, 'The root can be requested');
            let childrenFolder1 = await browser.bookmarks.getChildren(folder.id);
            browser.test.assertTrue(Array.isArray(childrenFolder1), 'childrenFolder1 should be an array');
            browser.test.assertEq('Folder 2', childrenFolder1[1].title, 'childs title should be Folder 2');
            let getFolder1 = await browser.bookmarks.get([folder.id, folder2.id]);
            browser.test.assertTrue(Array.isArray(getFolder1), 'getFolder1 should be an array');
            browser.test.assertEq(2, getFolder1.length, 'getFolder1 array should have 2 elements');
            browser.test.assertEq('Folder 2', getFolder1[1].title, 'childs title should be Folder 2');
            browser.test.assertEq('Test Folder', getFolder1[0].title, 'first title should be Test Folder');
            browser.test.notifyPass();
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func removeAndRemoveTree() async throws {
        let backgroundScript = """
            let folder1 = await browser.bookmarks.create({ title: 'Folder One' });
            let bookmarkA = await browser.bookmarks.create({ parentId: folder1.id, title: 'Bookmark A', url: 'http://example.com/a' });
            let folder2 = await browser.bookmarks.create({ parentId: folder1.id, title: 'Folder Two' });
            let bookmarkB = await browser.bookmarks.create({ parentId: folder2.id, title: 'Bookmark B', url: 'http://example.com/b' });
            let topLevelBookmark = await browser.bookmarks.create({ title: 'Top Level Bookmark', url: 'http://example.com/top' });
            let tree1 = (await browser.bookmarks.getTree())[0].children[0].children;
            browser.test.assertEq(2, tree1[0].children.length, 'folder1 should have 2 children');
            browser.test.assertEq('Top Level Bookmark', tree1[1].title, 'Second top-level is topBookmark');
            browser.test.assertEq('Bookmark A', tree1[0].children[0].title, 'bookmarkA is child of folder1');

            await browser.test.assertRejects(
            browser.bookmarks.remove(folder2.id),
            /the folder is not empty/i,
            'FAIL-2: remove on a non-empty folder should fail with the correct message');

            await browser.test.assertRejects(
            browser.bookmarks.remove('nonexistent-id-123'),
            /bookmark not found/i,
            'FAIL-3a: remove on a non-existent ID should fail with the correct message');

            await browser.test.assertRejects(
            browser.bookmarks.removeTree('nonexistent-id-456'),
            /bookmark not found/i,
            'FAIL-3b: removeTree on a non-existent ID should fail with the correct message');

            await browser.bookmarks.remove(bookmarkA.id);
            let tree2 = (await browser.bookmarks.getTree())[0].children[0].children;
            browser.test.assertEq(1, tree2[0].children.length, 'folder1 should now have 1 child after removing bookmarkA');
            browser.test.assertEq('Folder Two', tree2[0].children[0].title, 'folder2 should be the only child of folder1');
            await browser.bookmarks.removeTree(folder2.id);
            let tree3 = (await browser.bookmarks.getTree())[0].children[0].children;
            browser.test.assertEq(0, tree3[0].children.length, 'folder1 should now have 0 children after removing folder2');
            browser.test.assertEq(2, tree3.length, 'Tree should still have 2 top-level items (folder1 and topBookmark)');
            browser.test.assertEq('Folder One', tree3[0].title, 'folder1 is still present');
            browser.test.assertEq(0, tree3[0].children.length, 'folder1 is still present but has no children');
            await browser.bookmarks.remove(folder1.id);
            let tree4 = (await browser.bookmarks.getTree())[0].children[0].children;
            browser.test.assertEq(1, tree4.length, 'the top level now ONLY has the top level bookmark');
            browser.test.notifyPass();
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func update() async throws {
        let backgroundScript = """
            let bm = await browser.bookmarks.create({ title: 'Initial Title', url: 'http://example.com/initial' });
            browser.test.assertEq('Initial Title', bm.title, 'Initial title check');
            browser.test.assertEq('http://example.com/initial', bm.url, 'Initial URL check');

            let updatedBm1 = await browser.bookmarks.update(bm.id, { title: 'Updated Title' });
            browser.test.assertEq('Updated Title', updatedBm1.title, 'Title updated correctly');
            browser.test.assertEq('http://example.com/initial', updatedBm1.url, 'URL unchanged after title update');

            let updatedBm2 = await browser.bookmarks.update(bm.id, { url: 'http://example.com/updated' });
            browser.test.assertEq('Updated Title', updatedBm2.title, 'Title unchanged after URL update');
            browser.test.assertEq('http://example.com/updated', updatedBm2.url, 'URL updated correctly');

            let updatedBm3 = await browser.bookmarks.update(bm.id, { title: 'Final Title', url: 'http://example.com/final' });
            browser.test.assertEq('Final Title', updatedBm3.title, 'Final title updated correctly');
            browser.test.assertEq('http://example.com/final', updatedBm3.url, 'Final URL updated correctly');

            let folder = await browser.bookmarks.create({ title: 'Initial Folder' });
            browser.test.assertEq('Initial Folder', folder.title, 'Initial title check');
            let updateFolder = await browser.bookmarks.update(folder.id, { title: 'Final Folder Title' });
            browser.test.assertEq('Final Folder Title', updateFolder.title, 'Final title updated correctly');

            await browser.test.assertRejects(
            browser.bookmarks.update(folder.id, { url: 'http://example.com/should-fail' }),
            /the URL of a folder cannot be set/i,
            'FAIL: Updating a folder URL should be rejected with the correct error message');

            await browser.test.assertRejects(
            browser.bookmarks.update('nonexistent-id', { title: 'New Title' }),
            /bookmark not found/i,
            'FAIL: Updating a non-existent ID should be rejected with the correct error message');

            browser.test.notifyPass();
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func move() async throws {
        let backgroundScript = """
            let folderA = await browser.bookmarks.create({title: 'Folder A' });
            let folderB = await browser.bookmarks.create({title: 'Folder B' });
            let bmTop = await browser.bookmarks.create({title: 'Top Bookmark', url: 'http://top.com' });
            let bmA = await browser.bookmarks.create({title: 'Bookmark A', url: 'http://a.com', parentId: folderA.id });
            let bmC = await browser.bookmarks.create({title: 'Bookmark C', url: 'http://c.com', parentId: folderA.id });
            let bmB = await browser.bookmarks.create({title: 'Bookmark B', url: 'http://b.com', parentId: folderB.id });

            let movedBm1 = await browser.bookmarks.move(bmA.id, { parentId: folderB.id });
            browser.test.assertEq(bmA.id, movedBm1.id, 'Moved bookmark ID should match');
            browser.test.assertEq(folderB.id, movedBm1.parentId, 'bmA parentId should be Folder B');
            browser.test.assertEq(1, movedBm1.index, 'bmA should be at index 1 in Folder B');

            let bmD = await browser.bookmarks.create({ title: 'Bookmark D', url: 'http://d.com', parentId: folderA.id });
            let movedBm2 = await browser.bookmarks.move(bmC.id, { parentId: folderA.id, index: 2 });
            browser.test.assertEq(bmC.id, movedBm2.id, 'Moved bookmark ID should match');
            browser.test.assertEq(folderA.id, movedBm2.parentId, 'bmC parentId should be Folder A');
            browser.test.assertEq(1, movedBm2.index, 'bmC should be at index 1 in Folder A');

            let movedFolder = await browser.bookmarks.move(folderB.id, { index: 0 });
            browser.test.assertEq(folderB.id, movedFolder.id, 'Moved folder ID should match');
            browser.test.assertEq('other', movedFolder.parentId, 'Folder B should stay in its folder');
            browser.test.assertEq(0, movedFolder.index, 'Folder B should be first in its folder');
            let movedFolder2 = await browser.bookmarks.move(folderB.id, { parentId: folderA.id });
            browser.test.assertEq(folderB.id, movedFolder2.id, 'Moved folder ID should match');
            browser.test.assertEq(folderA.id, movedFolder2.parentId, 'Folder B should be a child of Folder A');

            await browser.test.assertRejects(
            browser.bookmarks.move('nonexistent-id', { parentId: folderA.id }),
            /bookmark not found/i,
            'FAIL-1: Moving a non-existent bookmark should reject with the correct message');

            browser.test.log('Testing move to a non-folder parent...');
            await browser.test.assertRejects(
            browser.bookmarks.move(bmB.id, { parentId: bmD.id }),
            /the new parent is not a folder/i,
            'FAIL-2: Moving to a non-folder parent should reject with the correct message');

            browser.test.notifyPass();
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func search() async throws {
        let backgroundScript = """
            let folderA = await browser.bookmarks.create({ id: 'folderA_id', title: 'Animals Folder' });
            let bm1 = await browser.bookmarks.create({ id: 'bm1_id', parentId: folderA.id, title: 'Dog', url: 'http://animals.com/dog' });
            let bm2 = await browser.bookmarks.create({ id: 'bm2_id', parentId: folderA.id, title: 'Cat', url: 'http://animals.com/cat' });
            let bm3 = await browser.bookmarks.create({ id: 'bm3_id', parentId: folderA.id, title: 'Bird', url: 'http://birds.org/sparrow' });

            let folderB = await browser.bookmarks.create({ id: 'folderB_id', title: 'Plants Folder' });
            let bm4 = await browser.bookmarks.create({ id: 'bm4_id', parentId: folderB.id, title: 'Rose', url: 'http://flowers.com/rose' });
            let bm5 = await browser.bookmarks.create({ id: 'bm5_id', parentId: folderB.id, title: 'Lily', url: 'http://flowers.com/lily' });

            let bm6 = await browser.bookmarks.create({ id: 'bm6_id', title: 'Wild Animal', url: 'http://nature.com/wildlife' });
            let bm7 = await browser.bookmarks.create({ id: 'bm7_id', title: 'My Favorite Dog Site', url: 'http://dogs.com/favorite' });
            let bm8 = await browser.bookmarks.create({ id: 'bm8_id', title: 'A Bookmark', url: 'http://a.com/bookmark' });
            let bm9 = await browser.bookmarks.create({ id: 'bm9_id', title: 'Another Bookmark', url: 'http://another.com/bookmark' });
            let bm10 = await browser.bookmarks.create({ id: 'bm10_id', title: 'Special Folder' });
            let bm11 = await browser.bookmarks.create({ id: 'bm11_id', title: 'Special Link', url: 'http://specialLink.com' });

            let results1 = await browser.bookmarks.search('dog');
            browser.test.assertEq(2, results1.length, 'S1: Should find 2 bookmarks for dog');
            browser.test.assertEq(bm1.id, results1[0].id, 'S1: First result should be bm1');
            browser.test.assertEq(bm7.id, results1[1].id, 'S1: Second result should be bm7');

            let results2 = await browser.bookmarks.search('dog site');
            browser.test.assertEq(1, results2.length, 'S2: Should find 1 bookmark for dog site');
            browser.test.assertEq(bm7.id, results2[0].id, 'S2: Result should be bm7');

            let results3 = await browser.bookmarks.search('Animals');
            browser.test.assertEq(3, results3.length, 'S3: Should find 1 folder for Animals');
            browser.test.assertEq(folderA.id, results3[0].id, 'S3: Result should be folderA');
            browser.test.assertEq(bm1.id, results3[1].id, 'S3: Result should be bm1 since animals appears in the url');
            browser.test.assertEq(bm2.id, results3[2].id, 'S3: Result should be bm2 since animals appears in the url and is after bm1');

            let results4 = await browser.bookmarks.search('nonexistent');
            browser.test.assertEq(0, results4.length, 'S4: Should find 0 bookmarks for nonexistent');

            let results5 = await browser.bookmarks.search({ query: 'cat' });
            browser.test.assertEq(1, results5.length, 'S5: Should find 1 bookmark for query cat');
            browser.test.assertEq(bm2.id, results5[0].id, 'S5: Result should be bm2');

            let results6 = await browser.bookmarks.search({ title: 'Bird' });
            browser.test.assertEq(1, results6.length, 'S6: Should find 1 bookmark for title Bird');
            browser.test.assertEq(bm3.id, results6[0].id, 'S6: Result should be bm3');
            let results6_partial = await browser.bookmarks.search({ title: 'Bir' });
            browser.test.assertEq(0, results6_partial.length, 'S6: Should find 0 bookmarks for partial title Bir');
            let results6_case = await browser.bookmarks.search({ title: 'bird' });
            browser.test.assertEq(0, results6_case.length, 'S6: Should find 0 bookmarks for case-sensitive title bird');

            let results7 = await browser.bookmarks.search({ url: 'flowers.com' });
            browser.test.assertEq(0, results7.length, 'S7: Should find 2 bookmarks for url flowers.com');
            let results8 = await browser.bookmarks.search('flowers.com');
            browser.test.assertEq(2, results8.length, 'S8: Should find 2 bookmarks for url flowers.com');
            browser.test.assertEq(bm4.id, results8[0].id, 'S8: First result should be bm4');
            browser.test.assertEq(bm5.id, results8[1].id, 'S8: Second result should be bm5');

            let results9 = await browser.bookmarks.search({ title: 'Dog', url: 'http://animals.com/dog' });
            browser.test.assertEq(1, results9.length, 'S9: Should find 1 bookmark for title Dog and url animals.com');
            browser.test.assertEq(bm1.id, results9[0].id, 'S9: Result should be bm1');

            let results9p2 = await browser.bookmarks.search({ title: 'Dog', url: 'https://animals.com/dog' });
            browser.test.assertEq(0, results9p2.length, 'S9: Should find 0 bookmarks with https');

            let results10 = await browser.bookmarks.search({ query: 'nature', title: 'Wild Animal' });
            browser.test.assertEq(1, results10.length, 'S10: Should find 1 bookmark for query animal and title Wild Animal');
            browser.test.assertEq(bm6.id, results10[0].id, 'S10: Result should be bm6');

            let results11 = await browser.bookmarks.search({});
            browser.test.assertEq(13, results11.length, 'S11: Should return all 10 items for empty query');

            let results12 = await browser.bookmarks.search({ query: 'bookmark A' });
            browser.test.assertEq(2, results12.length, 'S12: Should find 2 bookmarks for query bookmark a');
            browser.test.assertEq(bm8.id, results12[0].id, 'S12: First result should be bm8');
            browser.test.assertEq(bm9.id, results12[1].id, 'S12: Second result should be bm9');

            let results13 = await browser.bookmarks.search({ query: 'Special' });
            browser.test.assertEq(2, results13.length, 'S13: Should find 2 bookmarks for query Speical');
            browser.test.assertEq(bm10.id, results13[0].id, 'S13: First result should be bm10');
            browser.test.assertEq(bm11.id, results13[1].id, 'S13: Second result should be bm11');
            let results14 = await browser.bookmarks.search({ query: 'Special', url: 'http://specialLink.com' });
            browser.test.assertEq(1, results14.length, 'S14: Should find 1 bookmarks for query Speical');
            browser.test.assertEq(bm11.id, results14[0].id, 'S14: First result should be bm11');

            browser.test.notifyPass();
            """

        let resources = ["background.js": backgroundScript]

        let manager = manager(resources: resources, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func rootNodeIdentifier() async throws {
        let backgroundScript = """
            browser.test.assertEq('0', browser.bookmarks.ROOT_NODE_ID, 'The root identifier should be 0')
            let topLevel = await browser.bookmarks.getChildren(browser.bookmarks.ROOT_NODE_ID)
            browser.test.assertEq(1, topLevel.length, 'The root should have the top-level node')
            browser.test.assertEq('Other', topLevel[0].title, 'The top-level node should be the default folder')
            browser.test.assertEq(browser.bookmarks.ROOT_NODE_ID, topLevel[0].parentId, 'Top-level nodes should have the root as their parent')
            let roots = await browser.bookmarks.get(browser.bookmarks.ROOT_NODE_ID)
            browser.test.assertEq(1, roots.length, 'The root can be requested')
            browser.test.assertEq(browser.bookmarks.ROOT_NODE_ID, roots[0].id, 'The root should have the root identifier')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func unknownBookmarkIdentifier() async throws {
        let backgroundScript = """
            await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })

            await browser.test.assertRejects(
            browser.bookmarks.getSubTree('missing'),
            /bookmark not found/i,
            'getSubTree with an unknown identifier should fail');

            await browser.test.assertRejects(
            browser.bookmarks.getChildren('missing'),
            /bookmark not found/i,
            'getChildren with an unknown identifier should fail');

            await browser.test.assertRejects(
            browser.bookmarks.get('missing'),
            /bookmark not found/i,
            'get with an unknown identifier should fail');

            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func folderTypeManagedAndSynced() async throws {
        let backgroundScript = """
            let tree = await browser.bookmarks.getTree()
            let nodes = tree[0].children
            browser.test.assertEq(4, nodes.length, 'The root should have four top-level nodes')
            browser.test.assertEq('bookmarks-bar', nodes[0].folderType, 'The bookmarks bar should report its folder type')
            browser.test.assertTrue(nodes[0].unmodifiable === undefined, 'The bookmarks bar should be modifiable')
            browser.test.assertEq(true, nodes[0].syncing, 'The bookmarks bar should be syncing')
            browser.test.assertEq('managed', nodes[1].folderType, 'The managed folder should report its folder type')
            browser.test.assertEq('managed', nodes[1].unmodifiable, 'The managed folder should be unmodifiable')
            browser.test.assertEq(false, nodes[1].syncing, 'The managed folder should never be syncing')
            browser.test.assertEq('managed', nodes[1].children[0].unmodifiable, 'A managed bookmark should be unmodifiable')
            browser.test.assertEq(false, nodes[1].children[0].syncing, 'A managed bookmark should never be syncing, even if the delegate says so')
            browser.test.assertTrue(nodes[1].children[0].folderType === undefined, 'A bookmark should not have a folder type')
            browser.test.assertEq('other', nodes[2].folderType, 'The other folder should report its folder type')
            browser.test.assertEq(false, nodes[2].syncing, 'A node that does not say should not be syncing')
            browser.test.assertTrue(nodes[3].folderType === undefined, 'An ordinary folder should not have a folder type')
            let children = await browser.bookmarks.getChildren('managed')
            browser.test.assertEq('managed', children[0].unmodifiable, 'getChildren should report unmodifiable')
            let results = await browser.bookmarks.search({ title: 'Policy' })
            browser.test.assertEq(1, results.length, 'The search should find the managed bookmark')
            let folderResults = await browser.bookmarks.search('Favorites')
            browser.test.assertEq(0, folderResults.length, 'The search should not return special folders')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        let store = bookmarkStore
        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            let bar: NSDictionary = [
                "id": "bar", "title": "Favorites", "folderType": "bookmarks-bar", "syncing": true, "children": NSMutableArray(),
            ]
            let managedBookmark: NSDictionary = [
                "id": "managed-bookmark", "title": "Policy", "url": "https://webkit.org/", "managed": true, "syncing": true,
            ]
            let managed: NSDictionary = [
                "id": "managed", "title": "Managed", "folderType": "managed", "managed": true,
                "children": NSMutableArray(array: [managedBookmark]),
            ]
            let other: NSDictionary = ["id": "other", "title": "Other", "folderType": "other", "children": NSMutableArray()]
            let ordinary: NSDictionary = ["id": "ordinary", "title": "Reading", "children": NSMutableArray()]

            let nodes: [any _WKWebExtensionBookmark] = [bar, managed, other, ordinary].map {
                TestBookmarkNode(dictionary: $0, store: store)
            }

            completionHandler(nodes, nil)
        }

        try await manager.loadAndRun()
    }

    @Test
    func createUsesTheDefaultFolderAndIndex() async throws {
        let backgroundScript = """
            let first = await browser.bookmarks.create({ title: 'First', url: 'https://webkit.org/' })
            browser.test.assertEq('other', first.parentId, 'A bookmark without a parent should go in the default folder')
            browser.test.assertEq(0, first.index, 'The first bookmark should have index 0')
            let second = await browser.bookmarks.create({ title: 'Second', url: 'https://webkit.org/blog/' })
            browser.test.assertEq(1, second.index, 'A bookmark without an index should be added at the end')
            let inserted = await browser.bookmarks.create({ title: 'Inserted', url: 'https://webkit.org/status/', index: 1 })
            browser.test.assertEq(1, inserted.index, 'A bookmark with an index should be inserted there')
            let children = await browser.bookmarks.getChildren('other')
            browser.test.assertEq('First,Inserted,Second', children.map(child => child.title).join(','), 'The children should be in order')

            browser.test.assertThrows(() => browser.bookmarks.create({ title: 'Negative', url: 'https://webkit.org/', index: -1 }), /The 'index' value is invalid, because it must not be negative/i)
            await browser.test.assertRejects(
            browser.bookmarks.create({ title: 'Too Far', url: 'https://webkit.org/', index: 4 }),
            /index out of bounds/i,
            'An index past the end should fail');

            browser.test.assertThrows(() => browser.bookmarks.create({ title: 'Invalid', url: 'not a url' }), /The 'url' value is invalid, because it must be a valid URL/i)
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func createWithoutDefaultFolderFails() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(
            browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' }),
            /there is no default folder for new bookmarks/i,
            'A bookmark without a parent needs a default folder');
            let folder = await browser.bookmarks.create({ title: 'Folder', parentId: 'ordinary' })
            browser.test.assertEq('ordinary', folder.parentId, 'A bookmark with a parent should not need a default folder')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        bookmarkStore.topLevelBookmarks.removeAllObjects()
        bookmarkStore.topLevelBookmarks.add(
            NSMutableDictionary(dictionary: ["id": "ordinary", "title": "Ordinary", "type": "folder", "children": NSMutableArray()])
        )

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func mutationsRespectTheRootAndManagedBookmarks() async throws {
        let backgroundScript = """
            let bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })

            await browser.test.assertRejects(browser.bookmarks.create({ title: 'In Root', parentId: browser.bookmarks.ROOT_NODE_ID }), /the bookmark root cannot be modified/i, 'create in the root')
            await browser.test.assertRejects(browser.bookmarks.update(browser.bookmarks.ROOT_NODE_ID, { title: 'Renamed' }), /the bookmark root cannot be modified/i, 'update the root')
            await browser.test.assertRejects(browser.bookmarks.move(bookmark.id, { parentId: browser.bookmarks.ROOT_NODE_ID }), /the bookmark root cannot be modified/i, 'move into the root')
            await browser.test.assertRejects(browser.bookmarks.move(browser.bookmarks.ROOT_NODE_ID, { parentId: 'other' }), /the bookmark root cannot be modified/i, 'move the root')
            await browser.test.assertRejects(browser.bookmarks.remove(browser.bookmarks.ROOT_NODE_ID), /the bookmark root cannot be modified/i, 'remove the root')
            await browser.test.assertRejects(browser.bookmarks.removeTree(browser.bookmarks.ROOT_NODE_ID), /the bookmark root cannot be modified/i, 'remove the root tree')

            await browser.test.assertRejects(browser.bookmarks.create({ title: 'In Managed', parentId: 'managed' }), /managed bookmarks cannot be modified/i, 'create in a managed folder')
            await browser.test.assertRejects(browser.bookmarks.update('managed-bookmark', { title: 'Renamed' }), /managed bookmarks cannot be modified/i, 'update a managed bookmark')
            await browser.test.assertRejects(browser.bookmarks.move(bookmark.id, { parentId: 'managed' }), /managed bookmarks cannot be modified/i, 'move into a managed folder')
            await browser.test.assertRejects(browser.bookmarks.move('managed-bookmark', { parentId: 'other' }), /managed bookmarks cannot be modified/i, 'move a managed bookmark')
            await browser.test.assertRejects(browser.bookmarks.remove('managed-bookmark'), /managed bookmarks cannot be modified/i, 'remove a managed bookmark')
            await browser.test.assertRejects(browser.bookmarks.removeTree('managed'), /managed bookmarks cannot be modified/i, 'remove a managed folder')

            let remaining = await browser.bookmarks.getChildren('managed')
            browser.test.assertEq(1, remaining.length, 'The managed bookmark should still be there')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        bookmarkStore.topLevelBookmarks.add(
            NSMutableDictionary(dictionary: [
                "id": "managed", "title": "Managed", "type": "folder", "folderType": "managed", "managed": true,
                "children": NSMutableArray(array: [
                    NSMutableDictionary(dictionary: [
                        "id": "managed-bookmark", "title": "Policy", "url": "https://webkit.org/", "managed": true,
                    ])
                ]),
            ])
        )

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func moveRejectsInvalidDestinations() async throws {
        let backgroundScript = """
            let folder = await browser.bookmarks.create({ title: 'Folder' })
            let child = await browser.bookmarks.create({ title: 'Child', parentId: folder.id })
            let bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })

            await browser.test.assertRejects(browser.bookmarks.move(folder.id, { parentId: folder.id }), /a folder cannot be moved into itself or its descendants/i, 'into itself')
            await browser.test.assertRejects(browser.bookmarks.move(folder.id, { parentId: child.id }), /a folder cannot be moved into itself or its descendants/i, 'into a descendant')
            await browser.test.assertRejects(browser.bookmarks.move(child.id, { parentId: bookmark.id }), /the new parent is not a folder/i, 'into a bookmark')
            await browser.test.assertRejects(browser.bookmarks.move(child.id, { parentId: 'missing' }), /parent bookmark not found/i, 'into a missing folder')
            await browser.test.assertRejects(browser.bookmarks.move(child.id, { parentId: folder.id, index: 3 }), /index out of bounds/i, 'past the end')
            browser.test.assertThrows(() => browser.bookmarks.move(child.id, { index: -1 }), /The 'index' value is invalid, because it must not be negative/i)
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func removeRequiresAnEmptyFolderUnlessRecursive() async throws {
        let backgroundScript = """
            let folder = await browser.bookmarks.create({ title: 'Folder' })
            let child = await browser.bookmarks.create({ title: 'Child', parentId: folder.id, url: 'https://webkit.org/' })
            await browser.test.assertRejects(browser.bookmarks.remove(folder.id), /the folder is not empty, use removeTree\\(\\) to remove it/i, 'remove a folder with children')
            await browser.bookmarks.removeTree(child.id)
            let empty = await browser.bookmarks.getChildren(folder.id)
            browser.test.assertEq(0, empty.length, 'removeTree should remove a bookmark')
            await browser.bookmarks.remove(folder.id)
            await browser.test.assertRejects(browser.bookmarks.getChildren(folder.id), /bookmark not found/i, 'the folder should be gone')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func changesFailWhenTheNodeCannotDoThem() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.create({ title: 'Child', parentId: 'minimal' }), /it is not implemented/i, 'create')
            await browser.test.assertRejects(browser.bookmarks.update('minimal', { title: 'Renamed' }), /it is not implemented/i, 'update')
            await browser.test.assertRejects(browser.bookmarks.move('minimal', { parentId: 'minimal' }), /a folder cannot be moved into itself or its descendants/i, 'move into itself')
            await browser.test.assertRejects(browser.bookmarks.remove('minimal'), /it is not implemented/i, 'remove')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            completionHandler([TestMinimalBookmarkNode()], nil)
        }

        try await manager.loadAndRun()
    }

    @Test
    func delegateCompletesAfterReturning() async throws {
        let backgroundScript = """
            let created = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })
            browser.test.assertEq('WebKit', created.title, 'The created bookmark should be returned')
            let updated = await browser.bookmarks.update(created.id, { title: 'Blog' })
            browser.test.assertEq('Blog', updated.title, 'The updated bookmark should be returned')
            let folder = await browser.bookmarks.create({ title: 'Folder' })
            let moved = await browser.bookmarks.move(created.id, { parentId: folder.id })
            browser.test.assertEq(folder.id, moved.parentId, 'The moved bookmark should be in the folder')
            let tree = await browser.bookmarks.getTree()
            browser.test.assertEq(1, tree[0].children.length, 'The root should have one child')
            await browser.bookmarks.remove(created.id)
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        bookmarkStore.completesAsynchronously = true

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func getTreeWithBookmarkMissingOptionalMethods() async throws {
        let backgroundScript = """
            let tree = await browser.bookmarks.getTree()
            browser.test.assertEq(1, tree[0].children.length, 'The root should have one child')
            let node = tree[0].children[0]
            browser.test.assertEq('minimal', node.id, 'The identifier should come from the delegate')
            browser.test.assertEq('', node.title, 'The title should default to empty')
            browser.test.assertEq(0, node.index, 'The index should be its position')
            browser.test.assertEq('0', node.parentId, 'The parent should be the root')
            browser.test.assertEq('folder', node.type, 'A node without a URL should be a folder')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            completionHandler([TestMinimalBookmarkNode()], nil)
        }

        try await manager.loadAndRun()
    }

    @Test
    func invalidIdentifiersAndChanges() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.bookmarks.get(['1', '']), /The 'idOrIdList' value is invalid, because each item in the list must be a non-empty string/i)
            browser.test.assertThrows(() => browser.bookmarks.get(123), /The 'idOrIdList' value is invalid, because a string or an array of strings is expected/i)
            browser.test.assertThrows(() => browser.bookmarks.update('', { title: 'WebKit' }), /The 'id' value is invalid, because it must be a non-empty string/i)
            browser.test.assertThrows(() => browser.bookmarks.update('1', { }), /The 'changes' value is invalid, because it must contain a title or a url/i)
            browser.test.assertThrows(() => browser.bookmarks.search(123), /The 'query' value is invalid, because a string or an object is expected/i)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func changesReportTheErrorFromTheApp() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.create({ title: 'Docs', parentId: 'reading' }), /The bookmark database is locked/i, 'create')
            await browser.test.assertRejects(browser.bookmarks.update('webkit', { title: 'WebKit Home' }), /The bookmark database is locked/i, 'update title')
            await browser.test.assertRejects(browser.bookmarks.update('webkit', { url: 'https://example.org/' }), /The bookmark database is locked/i, 'update url')
            await browser.test.assertRejects(browser.bookmarks.move('webkit', { parentId: 'reading' }), /The bookmark database is locked/i, 'move')
            await browser.test.assertRejects(browser.bookmarks.remove('webkit'), /The bookmark database is locked/i, 'remove')
            await browser.test.assertRejects(browser.bookmarks.removeTree('reading'), /The bookmark database is locked/i, 'removeTree')
            browser.test.notifyPass()
            """

        let children = try #require((bookmarkStore.topLevelBookmarks[0] as? NSDictionary)?["children"] as? NSMutableArray)
        children.add(NSMutableDictionary(dictionary: ["id": "webkit", "title": "WebKit", "url": "https://webkit.org/"]))
        children.add(
            NSMutableDictionary(dictionary: ["id": "reading", "title": "Reading", "type": "folder", "children": NSMutableArray()])
        )
        bookmarkStore.failureMessage = "The bookmark database is locked"

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func createWithoutABookmarkOrAnErrorFails() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' }), /an unknown error occurred/i)
            browser.test.notifyPass()
            """

        bookmarkStore.createReturnsNoBookmark = true

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func getTreeFailsWhenTheAppReportsAnError() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.getTree(), /The bookmarks could not be read/i)
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            completionHandler(
                nil,
                NSError(domain: "TestBookmarks", code: 1, userInfo: [NSLocalizedDescriptionKey: "The bookmarks could not be read"])
            )
        }

        try await manager.loadAndRun()
    }

    @Test
    func getTreeFailsWithoutBookmarksOrAnError() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.getTree(), /an unknown error occurred/i)
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            completionHandler(nil, nil)
        }

        try await manager.loadAndRun()
    }

    @Test
    func bookmarksFailWhenTheControllerDoesNotProvideThem() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.getTree(), /it is not implemented/i, 'getTree')
            await browser.test.assertRejects(browser.bookmarks.get('1'), /it is not implemented/i, 'get')
            await browser.test.assertRejects(browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' }), /it is not implemented/i, 'create')
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func createInsideABookmarkFails() async throws {
        let backgroundScript = """
            const bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })
            await browser.test.assertRejects(browser.bookmarks.create({ title: 'Docs', parentId: bookmark.id }), /the parent is not a folder/i)
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func createRejectsAnUnknownType() async throws {
        let backgroundScript = """
            browser.test.assertThrows(() => browser.bookmarks.create({ title: 'Divider', type: 'separator' }), /it must specify either 'bookmark' or 'folder'/i)
            browser.test.notifyPass()
            """

        try await loadAndRun(manifest: bookmarksManifest, resources: ["background.js": backgroundScript])
    }

    @Test
    func updateNeedsTheBookmarkToSupportTheChange() async throws {
        let backgroundScript = """
            await browser.test.assertRejects(browser.bookmarks.update('read-only', { title: 'WebKit Home' }), /it is not implemented for 'title'/i, 'title')
            await browser.test.assertRejects(browser.bookmarks.update('read-only', { url: 'https://example.org/' }), /it is not implemented for 'url'/i, 'url')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        manager.internalDelegate.bookmarksForExtensionContext = { completionHandler in
            completionHandler([TestStaticBookmarkNode()], nil)
        }

        try await manager.loadAndRun()
    }

    @Test
    func bookmarksWithoutAnIdentifierAreSkipped() async throws {
        let backgroundScript = """
            const children = await browser.bookmarks.getChildren('other')
            browser.test.assertEq(1, children.length, 'Only the bookmark with an identifier should be listed')
            browser.test.assertEq('webkit', children[0].id)
            browser.test.notifyPass()
            """

        let children = try #require((bookmarkStore.topLevelBookmarks[0] as? NSDictionary)?["children"] as? NSMutableArray)
        children.add(NSMutableDictionary(dictionary: ["id": "", "title": "Nameless", "url": "https://webkit.org/blog/"]))
        children.add(NSMutableDictionary(dictionary: ["id": "webkit", "title": "WebKit", "url": "https://webkit.org/"]))

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func updateKeepsTheURLForEmptyOrInvalidValues() async throws {
        let backgroundScript = """
            const bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })
            const unchanged = await browser.bookmarks.update(bookmark.id, { title: 'WebKit Home', url: '' })
            browser.test.assertEq('WebKit Home', unchanged.title, 'The title should change')
            browser.test.assertEq('https://webkit.org/', unchanged.url, 'An empty URL should leave the URL alone')
            browser.test.assertThrows(() => browser.bookmarks.update(bookmark.id, { url: 'this is not a valid url' }), /it must be a valid URL/i)
            const current = await browser.bookmarks.get(bookmark.id)
            browser.test.assertEq('https://webkit.org/', current[0].url, 'An invalid URL should not change the URL')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func createWithAnEmptyURLCreatesAFolder() async throws {
        let backgroundScript = """
            const folder = await browser.bookmarks.create({ title: 'Reading', url: '' })
            browser.test.assertEq('folder', folder.type, 'An empty URL should create a folder')
            browser.test.assertEq(undefined, folder.url, 'A folder should not have a URL')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func getReturnsBookmarksInTheRequestedOrder() async throws {
        let backgroundScript = """
            const first = await browser.bookmarks.create({ title: 'First', url: 'https://webkit.org/' })
            const second = await browser.bookmarks.create({ title: 'Second', url: 'https://webkit.org/blog/' })
            const nodes = await browser.bookmarks.get([second.id, first.id])
            browser.test.assertEq(2, nodes.length)
            browser.test.assertEq(second.id, nodes[0].id, 'The first result should be the first identifier')
            browser.test.assertEq(first.id, nodes[1].id, 'The second result should be the second identifier')
            await browser.test.assertRejects(browser.bookmarks.get([first.id, 'missing']), /bookmark not found/i, 'One unknown identifier should fail the call')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func getChildrenAndGetSubTreeShapes() async throws {
        let backgroundScript = """
            const parent = await browser.bookmarks.create({ title: 'Parent' })
            const child = await browser.bookmarks.create({ title: 'Child', parentId: parent.id })
            const leaf = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/', parentId: child.id })
            const empty = await browser.bookmarks.create({ title: 'Empty' })
            const children = await browser.bookmarks.getChildren(parent.id)
            browser.test.assertEq(1, children.length, 'Only the immediate children should be returned')
            browser.test.assertEq(undefined, children[0].children, 'The children should not include their own children')
            const [subTree] = await browser.bookmarks.getSubTree(parent.id)
            browser.test.assertEq(leaf.id, subTree.children[0].children[0].id, 'The subtree should include every descendant')
            const [emptySubTree] = await browser.bookmarks.getSubTree(empty.id)
            browser.test.assertEq(0, emptySubTree.children.length, 'An empty folder should have an empty list of children')
            const [leafSubTree] = await browser.bookmarks.getSubTree(leaf.id)
            browser.test.assertEq(leaf.id, leafSubTree.id, 'The subtree of a bookmark should be the bookmark')
            browser.test.assertEq(0, (await browser.bookmarks.getChildren(leaf.id)).length, 'A bookmark should have no children')
            await browser.test.assertRejects(browser.bookmarks.getChildren('missing'), /bookmark not found/i, 'getChildren')
            await browser.test.assertRejects(browser.bookmarks.getSubTree('missing'), /bookmark not found/i, 'getSubTree')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func removedBookmarksLeaveGetAndSearch() async throws {
        let backgroundScript = """
            const folder = await browser.bookmarks.create({ title: 'Reading' })
            const nested = await browser.bookmarks.create({ title: 'Nested Folder', parentId: folder.id })
            const inner = await browser.bookmarks.create({ title: 'Inner Link', url: 'https://webkit.org/blog/', parentId: nested.id })
            const leaf = await browser.bookmarks.create({ title: 'Outer Link', url: 'https://webkit.org/' })
            const empty = await browser.bookmarks.create({ title: 'Empty' })
            await browser.bookmarks.remove(leaf.id)
            browser.test.assertEq(0, (await browser.bookmarks.search('Outer Link')).length, 'A removed bookmark should not be found')
            await browser.test.assertRejects(browser.bookmarks.get(leaf.id), /bookmark not found/i, 'A removed bookmark should not be returned')
            await browser.bookmarks.remove(empty.id)
            await browser.test.assertRejects(browser.bookmarks.get(empty.id), /bookmark not found/i, 'A removed empty folder should not be returned')
            await browser.bookmarks.removeTree(folder.id)
            await browser.test.assertRejects(browser.bookmarks.get(inner.id), /bookmark not found/i, 'A descendant should be removed with the folder')
            browser.test.assertEq(0, (await browser.bookmarks.search('Inner Link')).length, 'A removed descendant should not be found')
            const another = await browser.bookmarks.create({ title: 'Another Link', url: 'https://webkit.org/status/' })
            await browser.bookmarks.removeTree(another.id)
            await browser.test.assertRejects(browser.bookmarks.get(another.id), /bookmark not found/i, 'removeTree should remove a bookmark')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func movingAFolderKeepsItsChildren() async throws {
        let backgroundScript = """
            const source = await browser.bookmarks.create({ title: 'Source' })
            const destination = await browser.bookmarks.create({ title: 'Destination' })
            const link = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/', parentId: source.id })
            const moved = await browser.bookmarks.move(source.id, { parentId: destination.id })
            browser.test.assertEq(destination.id, moved.parentId, 'The folder should have the new parent')
            const children = await browser.bookmarks.getChildren(source.id)
            browser.test.assertEq(1, children.length, 'The folder should keep its children')
            browser.test.assertEq(link.id, children[0].id)
            await browser.test.assertRejects(browser.bookmarks.move(link.id, { parentId: 'missing' }), /parent bookmark not found/i, 'A move to an unknown parent should fail')
            browser.test.notifyPass()
            """

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }

    @Test
    func updateRepliesWithTheChangedBookmark() async throws {
        let backgroundScript = """
            const bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })
            const renamed = await browser.bookmarks.update(bookmark.id, { title: 'WebKit Home' })
            browser.test.assertEq('WebKit Home', renamed.title, 'The reply should have the new title')
            const moved = await browser.bookmarks.update(bookmark.id, { url: 'https://webkit.org/blog/' })
            browser.test.assertEq('https://webkit.org/blog/', moved.url, 'The reply should have the new URL')
            browser.test.assertEq('WebKit Home', moved.title, 'The reply should keep the earlier title')
            browser.test.notifyPass()
            """

        bookmarkStore.nodesAreSnapshots = true

        let manager = manager(backgroundScript: backgroundScript, manifest: bookmarksManifest)

        configureBookmarksDelegate(manager)

        try await manager.loadAndRun()
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS_BOOKMARKS
