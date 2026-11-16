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

#import "CocoaHelpers.h"
#import "config.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionPermissionPrivate.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebExtensionBookmarkConfiguration.h>
#import <WebKit/_WKWebExtensionBookmarks.h>

// Holds the bookmarks that the nodes read and change.
@interface TestBookmarkStore : NSObject
@property (nonatomic, strong) NSMutableArray<NSMutableDictionary *> *topLevelBookmarks;
@property (nonatomic) NSInteger nextIdentifier;
@property (nonatomic) BOOL completesAsynchronously;
@property (nonatomic, copy) NSString *failureMessage;
@property (nonatomic) BOOL createReturnsNoBookmark;
@property (nonatomic) BOOL nodesAreSnapshots;
- (NSError *)failure;
- (NSMutableArray<NSMutableDictionary *> *)siblingsOfBookmark:(NSMutableDictionary *)bookmark;
- (void)complete:(dispatch_block_t)block;
@end

@interface TestBookmarkNode : NSObject <_WKWebExtensionBookmark>
- (instancetype)initWithDictionary:(NSDictionary *)dictionary store:(TestBookmarkStore *)store;
- (instancetype)initWithDictionary:(NSDictionary *)dictionary;
@property (nonatomic, strong) NSMutableDictionary *dictionary;
@property (nonatomic, strong) NSMutableDictionary *originalDictionary;
@property (nonatomic, strong) TestBookmarkStore *store;
@end

@implementation TestBookmarkStore

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.topLevelBookmarks = [NSMutableArray array];
        _nextIdentifier = 100;
    }
    return self;
}

static NSMutableArray<NSMutableDictionary *> *siblingsOfBookmarkInArray(NSMutableArray<NSMutableDictionary *> *bookmarks, NSMutableDictionary *target)
{
    if ([bookmarks indexOfObjectIdenticalTo:target] != NSNotFound)
        return bookmarks;

    for (NSMutableDictionary *bookmark in bookmarks) {
        id children = bookmark[@"children"];
        if (![children isKindOfClass:NSMutableArray.class])
            continue;

        if (NSMutableArray *found = siblingsOfBookmarkInArray(children, target))
            return found;
    }

    return nil;
}

- (NSMutableArray<NSMutableDictionary *> *)siblingsOfBookmark:(NSMutableDictionary *)bookmark
{
    return siblingsOfBookmarkInArray(_topLevelBookmarks, bookmark);
}

- (NSError *)failure
{
    if (!self.failureMessage)
        return nil;

    return [NSError errorWithDomain:@"TestBookmarks" code:1 userInfo:@{ NSLocalizedDescriptionKey: self.failureMessage }];
}

- (void)complete:(dispatch_block_t)block
{
    if (_completesAsynchronously) {
        dispatch_async(mainDispatchQueueSingleton(), block);
        return;
    }

    block();
}

@end

@implementation TestBookmarkNode

- (instancetype)initWithDictionary:(NSDictionary *)dictionary store:(TestBookmarkStore *)store
{
    self = [super init];
    if (self) {
        self.dictionary = [dictionary isKindOfClass:NSMutableDictionary.class] ? (NSMutableDictionary *)dictionary : adoptNS([dictionary mutableCopy]).get();
        self.store = store;

        // A snapshot node answers from a copy and sends changes to the stored bookmark, like an app whose nodes are values.
        if (store.nodesAreSnapshots && [dictionary isKindOfClass:NSMutableDictionary.class]) {
            self.originalDictionary = (NSMutableDictionary *)dictionary;
            self.dictionary = adoptNS([dictionary mutableCopy]).get();
        }
    }
    return self;
}

- (instancetype)initWithDictionary:(NSDictionary *)dictionary
{
    return [self initWithDictionary:dictionary store:nil];
}

- (NSMutableArray<NSMutableDictionary *> *)mutableChildren
{
    if (!_dictionary[@"children"])
        _dictionary[@"children"] = [NSMutableArray array];

    return _dictionary[@"children"];
}

- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"id"];
}

- (NSString *)titleForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"title"];
}

- (NSURL *)urlForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSString *url = _dictionary[@"url"];
    return url.length ? [NSURL URLWithString:url] : nil;
}

- (_WKWebExtensionBookmarkFolderType)folderTypeForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSString *folderType = _dictionary[@"folderType"];
    if ([folderType isEqualToString:@"bookmarks-bar"])
        return _WKWebExtensionBookmarkFolderTypeBookmarksBar;

    if ([folderType isEqualToString:@"managed"])
        return _WKWebExtensionBookmarkFolderTypeManaged;

    if ([folderType isEqualToString:@"other"])
        return _WKWebExtensionBookmarkFolderTypeOther;

    return _WKWebExtensionBookmarkFolderTypeNone;
}

- (BOOL)isManagedForWebExtensionContext:(WKWebExtensionContext *)context
{
    return [_dictionary[@"managed"] boolValue];
}

- (BOOL)isSyncedForWebExtensionContext:(WKWebExtensionContext *)context
{
    return [_dictionary[@"syncing"] boolValue];
}

- (_WKWebExtensionBookmarkType)bookmarkTypeForWebExtensionContext:(WKWebExtensionContext *)context
{
    if ([_dictionary[@"type"] isEqualToString:@"folder"])
        return _WKWebExtensionBookmarkTypeFolder;

    NSString *url = _dictionary[@"url"];
    return url.length ? _WKWebExtensionBookmarkTypeBookmark : _WKWebExtensionBookmarkTypeFolder;
}

- (NSArray<id<_WKWebExtensionBookmark>> *)childrenForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSArray *childDictionaries = _dictionary[@"children"];
    if (!childDictionaries)
        return nil;

    NSMutableArray<id<_WKWebExtensionBookmark>> *children = [NSMutableArray array];
    for (NSDictionary *childDictionary in childDictionaries)
        [children addObject:[[TestBookmarkNode alloc] initWithDictionary:childDictionary store:_store]];

    return children;
}

- (NSDate *)dateAddedForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSNumber *dateValue = WebKit::objectForKey<NSNumber>(_dictionary, @"dateAdded");
    if (!dateValue)
        return nil;

    return [NSDate dateWithTimeIntervalSince1970:dateValue.doubleValue / 1000.0];
}

- (void)createChildUsingConfiguration:(_WKWebExtensionBookmarkConfiguration *)configuration forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(id<_WKWebExtensionBookmark>, NSError *))completionHandler
{
    if (_store.failure || _store.createReturnsNoBookmark) {
        [_store complete:^{
            completionHandler(nil, _store.failure);
        }];
        return;
    }

    auto *newDictionary = [NSMutableDictionary dictionary];
    newDictionary[@"id"] = [NSString stringWithFormat:@"%ld", (long)_store.nextIdentifier++];
    newDictionary[@"title"] = configuration.title;
    newDictionary[@"dateAdded"] = @(NSDate.date.timeIntervalSince1970 * 1000.0);

    if (configuration.type == _WKWebExtensionBookmarkTypeBookmark)
        newDictionary[@"url"] = configuration.url.absoluteString;
    else {
        newDictionary[@"type"] = @"folder";
        newDictionary[@"children"] = [NSMutableArray array];
    }

    auto *children = self.mutableChildren;
    [children insertObject:newDictionary atIndex:std::min(configuration.index, children.count)];

    [_store complete:^{
        completionHandler([[TestBookmarkNode alloc] initWithDictionary:newDictionary store:_store], nil);
    }];
}

- (void)setTitle:(NSString *)title forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_store.failure) {
        [_store complete:^{
            completionHandler(_store.failure);
        }];
        return;
    }

    (_originalDictionary ?: _dictionary)[@"title"] = title;

    [_store complete:^{
        completionHandler(nil);
    }];
}

- (void)setURL:(NSURL *)url forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_store.failure) {
        [_store complete:^{
            completionHandler(_store.failure);
        }];
        return;
    }

    (_originalDictionary ?: _dictionary)[@"url"] = url.absoluteString;

    [_store complete:^{
        completionHandler(nil);
    }];
}

- (void)moveToParent:(id<_WKWebExtensionBookmark>)parent atIndex:(NSUInteger)index forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_store.failure) {
        [_store complete:^{
            completionHandler(_store.failure);
        }];
        return;
    }

    auto *newParent = (TestBookmarkNode *)parent;
    auto *oldSiblings = [_store siblingsOfBookmark:_dictionary];
    auto *newSiblings = newParent.mutableChildren;

    NSUInteger oldIndex = [oldSiblings indexOfObjectIdenticalTo:_dictionary];
    [oldSiblings removeObjectAtIndex:oldIndex];

    if (oldSiblings == newSiblings && oldIndex < index)
        --index;

    [newSiblings insertObject:_dictionary atIndex:std::min(index, newSiblings.count)];

    [_store complete:^{
        completionHandler(nil);
    }];
}

- (void)removeForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_store.failure) {
        [_store complete:^{
            completionHandler(_store.failure);
        }];
        return;
    }

    [[_store siblingsOfBookmark:_dictionary] removeObjectIdenticalTo:_dictionary];

    [_store complete:^{
        completionHandler(nil);
    }];
}

@end

@interface TestMinimalBookmarkNode : NSObject <_WKWebExtensionBookmark>
@end

@interface TestStaticBookmarkNode : NSObject <_WKWebExtensionBookmark>
@end

@implementation TestMinimalBookmarkNode

- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return @"minimal";
}

@end

@implementation TestStaticBookmarkNode

- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return @"read-only";
}

- (NSString *)titleForWebExtensionContext:(WKWebExtensionContext *)context
{
    return @"WebKit";
}

- (NSURL *)urlForWebExtensionContext:(WKWebExtensionContext *)context
{
    return [NSURL URLWithString:@"https://webkit.org/"];
}

@end

namespace TestWebKitAPI {

static constexpr auto *bookmarksNoPermissionManifest = @{
    @"manifest_version": @3,
    @"name": @"Bookmarks No Permission Test",
    @"description": @"Bookmarks No Permission Test",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

static constexpr auto *bookmarksManifest = @{
    @"manifest_version": @3,
    @"name": @"Bookmarks Test",
    @"description": @"Bookmarks Test",
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

    RetainPtr<TestBookmarkStore> bookmarkStore;

    void SetUp() override
    {
        testing::Test::SetUp();

        bookmarkStore = adoptNS([[TestBookmarkStore alloc] init]);
        [[bookmarkStore topLevelBookmarks] addObject:[@{ @"id": @"other", @"title": @"Other", @"type": @"folder", @"folderType": @"other", @"children": [NSMutableArray array] } mutableCopy]];
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, bookmarkConfig);
    }

    void configureBookmarksDelegate(TestWebExtensionManager *manager)
    {
        auto store = bookmarkStore;
        manager.internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
            [store complete:^{
                auto *nodes = [NSMutableArray array];
                for (NSMutableDictionary *dictionary in [store topLevelBookmarks])
                    [nodes addObject:adoptNS([[TestBookmarkNode alloc] initWithDictionary:dictionary store:store.get()]).get()];

                completionHandler(nodes, nil);
            }];
        };
    }

    WKWebExtensionControllerConfiguration *bookmarkConfig;
};

TEST_F(WKWebExtensionAPIBookmarks, APIUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.bookmarks, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarksNoPermissionManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

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

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, MissingArguments)
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

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, IncorrectArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren(123), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren({}), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.get('test', 'test'), /The 'callback' value is invalid, because a function is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.move(123, {}), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.move('someId', 'not-an-object'), /The 'destination' value is invalid, because an object is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.remove(123), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.search(123, 'test'), /The 'callback' value is invalid, because a function is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.update(123, {}), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, GetRecentWithInvalidArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent(-1), /The 'numberOfItems' value is invalid, because it must be at least 1./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent(0), /The 'numberOfItems' value is invalid, because it must be at least 1./i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, GetRecent)
{
    auto *script = @[
        @"let oldBm = await browser.bookmarks.create({title: 'Oldest Bookmark', url: 'http://example.com/1'})",
        @"let midBm = await browser.bookmarks.create({title: 'Middle Bookmark', url: 'http://example.com/2'})",
        @"let newBm = await browser.bookmarks.create({title: 'Newest Bookmark', url: 'http://example.com/3'})",
        @"let recent = await browser.bookmarks.getRecent(2)",
        @"browser.test.assertEq(2, recent.length, 'Should return exactly 2 bookmarks')",
        @"browser.test.assertEq('Newest Bookmark', recent[0].title, 'First result should be the newest bookmark')",
        @"browser.test.assertEq('Middle Bookmark', recent[1].title, 'Second result should be the middle bookmark')",
        @"let newFolder = await browser.bookmarks.create({title: 'Newest Folder'})",
        @"let recent2 = await browser.bookmarks.getRecent(5)",
        @"browser.test.assertEq(3, recent2.length, 'Should adapt and return the max available which is 3')",
        @"browser.test.assertEq('Newest Bookmark', recent2[0].title, 'First result should be the newest bookmark')",
        @"browser.test.assertEq('Middle Bookmark', recent2[1].title, 'Second result should be the middle bookmark')",
        @"browser.test.assertEq('Oldest Bookmark', recent2[2].title, 'Third result should be the oldest bookmark')",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, Create)
{
    auto *script = @[
        @"let createdNode = await browser.bookmarks.create({title: 'My Test Bookmark', url: 'https://example.com/test1'})",
        @"browser.test.assertEq('My Test Bookmark', createdNode.title, 'Title should match');",
        @"browser.test.assertEq('https://example.com/test1', createdNode.url, 'URL should match');",
        @"browser.test.assertEq('bookmark', createdNode.type, 'Type should be bookmark');",
        @"let createdNode2 = await browser.bookmarks.create({title: 'My Test Folder'})",
        @"browser.test.assertEq('My Test Folder', createdNode2.title, 'Title should match');",
        @"browser.test.assertEq('folder', createdNode2.type, 'type should be folder because url is not specified');",
        @"let createdNode3 = await browser.bookmarks.create({title: 'My Children Folder', parentId: createdNode2.id})",
        @"browser.test.assertEq('My Children Folder', createdNode3.title, 'Title should match');",
        @"browser.test.assertEq('folder', createdNode3.type, 'type should be folder because url is not specified');",
        @"browser.test.assertEq(createdNode2.id, createdNode3.parentId, 'parentId should match');",
        @"browser.test.assertEq('other', createdNode.parentId, 'A bookmark without a parent should be in the default folder');",
        @"browser.test.assertEq(0, createdNode.index, 'The first top-level bookmark should have index 0');",
        @"browser.test.assertEq(0, createdNode3.index, 'The first child should have index 0');",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetTree)
{
    auto *script = @[
        @"let bookmark1 = await browser.bookmarks.create({type: 'bookmark', title: 'Top Bookmark 1', url: 'http://example.com/bm1'});",
        @"let folder1 = await browser.bookmarks.create({type: 'folder', title: 'Top Folder 1'});",
        @"let bookmark2 = await browser.bookmarks.create({title: 'Child Bookmark 2', url: 'http://example.com/bm2', parentId: folder1.id});",
        @"let bookmark3 = await browser.bookmarks.create({title: 'Top Bookmark 3', url: 'http://example.com/bm3'});",
        @"let tree = await browser.bookmarks.getTree();",
        @"browser.test.assertTrue(Array.isArray(tree), 'The tree should be an array');",
        @"browser.test.assertEq(1, tree.length, 'The tree should have one root');",
        @"browser.test.assertEq('0', tree[0].id, 'The root should have the root identifier');",
        @"browser.test.assertEq('', tree[0].title, 'The root should have an empty title');",
        @"browser.test.assertEq('folder', tree[0].type, 'The root should be a folder');",
        @"browser.test.assertTrue(tree[0].parentId === undefined, 'The root should not have a parent');",
        @"let root = tree[0].children;",
        @"browser.test.assertEq(1, root.length, 'The root should have the default folder');",
        @"browser.test.assertEq('other', root[0].folderType, 'The default folder should report its folder type');",
        @"root = root[0].children;",
        @"browser.test.assertEq(3, root.length, 'The default folder should have the three new nodes');",
        @"let foundBookmark1 = root.find(n => n.title === bookmark1.title);",
        @"browser.test.assertEq('Top Bookmark 1', foundBookmark1.title, 'Bm1 title matches');",
        @"browser.test.assertEq('http://example.com/bm1', foundBookmark1.url, 'Bm1 URL matches');",
        @"browser.test.assertEq('bookmark', foundBookmark1.type, 'Bm1 type is bookmark');",
        @"let foundFolder1 = root.find(n => n.title === folder1.title);",
        @"browser.test.assertEq('Top Folder 1', foundFolder1.title, 'Folder1 title matches');",
        @"browser.test.assertEq('folder', foundFolder1.type, 'Folder1 type is folder');",
        @"browser.test.assertEq(bookmark2.title, foundFolder1.children[0].title, 'Folder1 should have children array');",
        @"let foundBookmark2 = foundFolder1.children.find(n => n.title === bookmark2.title);",
        @"browser.test.assertEq('Child Bookmark 2', foundBookmark2.title, 'Bm2 title matches');",
        @"browser.test.assertEq('http://example.com/bm2', foundBookmark2.url, 'Bm2 URL matches');",
        @"browser.test.assertEq('bookmark', foundBookmark2.type, 'Bm2 type is bookmark');",
        @"browser.test.assertEq(folder1.id, foundBookmark2.parentId, 'Bm2 parentId matches Folder1');",
        @"browser.test.assertEq('other', foundBookmark1.parentId, 'Bm1 parentId matches the default folder');",
        @"browser.test.assertEq(0, foundBookmark1.index, 'Bm1 index is its position in the default folder');",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());
    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, CreateAndGetTree)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({ title: 'Test Folder' });",
        @"browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');",
        @"browser.test.assertTrue(!!folder.id, 'Folder should have an ID');",
        @"let bookmark = await browser.bookmarks.create({ parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });",
        @"browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');",
        @"let rootNode = (await browser.bookmarks.getTree())[0].children[0].children;",
        @"browser.test.assertTrue(Array.isArray(rootNode), 'Root object should have a children array');",
        @"browser.test.assertEq(rootNode.length, 1);",
        @"browser.test.assertEq(rootNode[0].title, 'Test Folder');",
        @"browser.test.assertTrue(Array.isArray(rootNode[0].children), 'Folder should have a children array');",
        @"browser.test.assertEq(rootNode[0].children[0].title, 'WebKit.org', 'Child bookmark in tree should have correct title');",
        @"browser.test.assertEq(rootNode[0].children[0].url, 'https://webkit.org/', 'Child bookmark in tree should have correct URL');",
        @"let bookmark2 = await browser.bookmarks.create({ id: 'topLevelBookmark', title: 'Test Top Bookmark', url: 'https://coolbook.com/' });",
        @"browser.test.assertEq(bookmark2.title, 'Test Top Bookmark', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark2.url, 'https://coolbook.com/', 'Bookmark URL should be correct');",
        @"let updatedRootNode = (await browser.bookmarks.getTree())[0].children[0].children;",
        @"browser.test.assertEq(updatedRootNode.length, 2);",
        @"browser.test.assertEq(updatedRootNode[0].title, 'Test Folder');",
        @"browser.test.assertEq(updatedRootNode[1].title, 'Test Top Bookmark');",
        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());
    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetSubTreeChildren)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({title: 'Test Folder' });",
        @"browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');",
        @"browser.test.assertTrue(!!folder.id, 'Folder should have an ID');",
        @"let bookmark = await browser.bookmarks.create({parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });",
        @"browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');",
        @"let folder2 = await browser.bookmarks.create({parentId: folder.id, title: 'Folder 2'});",
        @"browser.test.assertEq(folder2.title, 'Folder 2', 'Folder title should be correct');",
        @"browser.test.assertEq(folder2.parentId, folder.id, 'Folder parentId should be correct');",
        @"let bookmark2 = await browser.bookmarks.create({parentId: folder2.id, title: 'Bookmark 2', url: 'https://bookmark2.org/' });",
        @"let subtreeFolder1 = await browser.bookmarks.getSubTree(folder.id);",
        @"browser.test.assertTrue(Array.isArray(subtreeFolder1), 'subtreeFolder1 should be an array');",
        @"browser.test.assertEq(1, subtreeFolder1.length, 'subtreeFolder1 array should only contain the requested node');",
        @"browser.test.assertEq(folder.id, subtreeFolder1[0].id, 'The requested node should be returned');",
        @"browser.test.assertEq(2, subtreeFolder1[0].children.length, 'The requested node should have 2 children');",
        @"browser.test.assertEq(bookmark.id, subtreeFolder1[0].children[0].id, 'First child is bookmark');",
        @"browser.test.assertEq('Folder 2', subtreeFolder1[0].children[1].title, 'Second child title should be Folder 2');",
        @"browser.test.assertEq('folder', subtreeFolder1[0].children[1].type, 'type should be folder');",
        @"browser.test.assertEq(1, subtreeFolder1[0].children[1].children.length, 'Folder 2 should have 1 child');",
        @"browser.test.assertEq(bookmark2.title, subtreeFolder1[0].children[1].children[0].title, 'bookmark2 should be child of folder2');",
        @"let subtreeBookmark = await browser.bookmarks.getSubTree(bookmark.id);",
        @"browser.test.assertTrue(Array.isArray(subtreeBookmark), 'subtreeBookmark should be an array');",
        @"browser.test.assertEq(1, subtreeBookmark.length, 'subtreeBookmark array should have 1 element');",
        @"browser.test.assertEq(bookmark.id, subtreeBookmark[0].id, 'The requested bookmark should be returned');",
        @"browser.test.assertTrue(subtreeBookmark[0].children === undefined, 'A bookmark should not have children');",
        @"let childrenOfBookmark = await browser.bookmarks.getChildren(bookmark.id);",
        @"browser.test.assertTrue(Array.isArray(childrenOfBookmark), 'childrenOfBookmark should be an array');",
        @"browser.test.assertEq(0, childrenOfBookmark.length, 'A bookmark should have no children');",
        @"let subtreeRoot = await browser.bookmarks.getSubTree('0');",
        @"browser.test.assertEq('0', subtreeRoot[0].id, 'The root can be requested');",
        @"let childrenFolder1 = await browser.bookmarks.getChildren(folder.id);",
        @"browser.test.assertTrue(Array.isArray(childrenFolder1), 'childrenFolder1 should be an array');",
        @"browser.test.assertEq('Folder 2', childrenFolder1[1].title, 'childs title should be Folder 2');",
        @"let getFolder1 = await browser.bookmarks.get([folder.id, folder2.id]);",
        @"browser.test.assertTrue(Array.isArray(getFolder1), 'getFolder1 should be an array');",
        @"browser.test.assertEq(2, getFolder1.length, 'getFolder1 array should have 2 elements');",
        @"browser.test.assertEq('Folder 2', getFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.assertEq('Test Folder', getFolder1[0].title, 'first title should be Test Folder');",
        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, RemoveAndRemoveTree)
{
    auto script = @[
        @"let folder1 = await browser.bookmarks.create({ title: 'Folder One' });",
        @"let bookmarkA = await browser.bookmarks.create({ parentId: folder1.id, title: 'Bookmark A', url: 'http://example.com/a' });",
        @"let folder2 = await browser.bookmarks.create({ parentId: folder1.id, title: 'Folder Two' });",
        @"let bookmarkB = await browser.bookmarks.create({ parentId: folder2.id, title: 'Bookmark B', url: 'http://example.com/b' });",
        @"let topLevelBookmark = await browser.bookmarks.create({ title: 'Top Level Bookmark', url: 'http://example.com/top' });",
        @"let tree1 = (await browser.bookmarks.getTree())[0].children[0].children;",
        @"browser.test.assertEq(2, tree1[0].children.length, 'folder1 should have 2 children');",
        @"browser.test.assertEq('Top Level Bookmark', tree1[1].title, 'Second top-level is topBookmark');",
        @"browser.test.assertEq('Bookmark A', tree1[0].children[0].title, 'bookmarkA is child of folder1');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.remove(folder2.id),",
        @"/the folder is not empty/i,",
        @"'FAIL-2: remove on a non-empty folder should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.remove('nonexistent-id-123'),",
        @"/bookmark not found/i,",
        @"'FAIL-3a: remove on a non-existent ID should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.removeTree('nonexistent-id-456'),",
        @"/bookmark not found/i,",
        @"'FAIL-3b: removeTree on a non-existent ID should fail with the correct message');",

        @"await browser.bookmarks.remove(bookmarkA.id);",
        @"let tree2 = (await browser.bookmarks.getTree())[0].children[0].children;",
        @"browser.test.assertEq(1, tree2[0].children.length, 'folder1 should now have 1 child after removing bookmarkA');",
        @"browser.test.assertEq('Folder Two', tree2[0].children[0].title, 'folder2 should be the only child of folder1');",
        @"await browser.bookmarks.removeTree(folder2.id);",
        @"let tree3 = (await browser.bookmarks.getTree())[0].children[0].children;",
        @"browser.test.assertEq(0, tree3[0].children.length, 'folder1 should now have 0 children after removing folder2');",
        @"browser.test.assertEq(2, tree3.length, 'Tree should still have 2 top-level items (folder1 and topBookmark)');",
        @"browser.test.assertEq('Folder One', tree3[0].title, 'folder1 is still present');",
        @"browser.test.assertEq(0, tree3[0].children.length, 'folder1 is still present but has no children');",
        @"await browser.bookmarks.remove(folder1.id);",
        @"let tree4 = (await browser.bookmarks.getTree())[0].children[0].children;",
        @"browser.test.assertEq(1, tree4.length, 'the top level now ONLY has the top level bookmark');",
        @"browser.test.notifyPass();",
    ];

    auto resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, Update)
{
    auto *script = @[
        @"let bm = await browser.bookmarks.create({ title: 'Initial Title', url: 'http://example.com/initial' });",
        @"browser.test.assertEq('Initial Title', bm.title, 'Initial title check');",
        @"browser.test.assertEq('http://example.com/initial', bm.url, 'Initial URL check');",

        @"let updatedBm1 = await browser.bookmarks.update(bm.id, { title: 'Updated Title' });",
        @"browser.test.assertEq('Updated Title', updatedBm1.title, 'Title updated correctly');",
        @"browser.test.assertEq('http://example.com/initial', updatedBm1.url, 'URL unchanged after title update');",

        @"let updatedBm2 = await browser.bookmarks.update(bm.id, { url: 'http://example.com/updated' });",
        @"browser.test.assertEq('Updated Title', updatedBm2.title, 'Title unchanged after URL update');",
        @"browser.test.assertEq('http://example.com/updated', updatedBm2.url, 'URL updated correctly');",

        @"let updatedBm3 = await browser.bookmarks.update(bm.id, { title: 'Final Title', url: 'http://example.com/final' });",
        @"browser.test.assertEq('Final Title', updatedBm3.title, 'Final title updated correctly');",
        @"browser.test.assertEq('http://example.com/final', updatedBm3.url, 'Final URL updated correctly');",

        @"let folder = await browser.bookmarks.create({ title: 'Initial Folder' });",
        @"browser.test.assertEq('Initial Folder', folder.title, 'Initial title check');",
        @"let updateFolder = await browser.bookmarks.update(folder.id, { title: 'Final Folder Title' });",
        @"browser.test.assertEq('Final Folder Title', updateFolder.title, 'Final title updated correctly');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.update(folder.id, { url: 'http://example.com/should-fail' }),",
        @"/the URL of a folder cannot be set/i,",
        @"'FAIL: Updating a folder URL should be rejected with the correct error message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.update('nonexistent-id', { title: 'New Title' }),",
        @"/bookmark not found/i,",
        @"'FAIL: Updating a non-existent ID should be rejected with the correct error message');",

        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, Move)
{
    auto *script = @[
        @"let folderA = await browser.bookmarks.create({title: 'Folder A' });",
        @"let folderB = await browser.bookmarks.create({title: 'Folder B' });",
        @"let bmTop = await browser.bookmarks.create({title: 'Top Bookmark', url: 'http://top.com' });",
        @"let bmA = await browser.bookmarks.create({title: 'Bookmark A', url: 'http://a.com', parentId: folderA.id });",
        @"let bmC = await browser.bookmarks.create({title: 'Bookmark C', url: 'http://c.com', parentId: folderA.id });",
        @"let bmB = await browser.bookmarks.create({title: 'Bookmark B', url: 'http://b.com', parentId: folderB.id });",

        @"let movedBm1 = await browser.bookmarks.move(bmA.id, { parentId: folderB.id });",
        @"browser.test.assertEq(bmA.id, movedBm1.id, 'Moved bookmark ID should match');",
        @"browser.test.assertEq(folderB.id, movedBm1.parentId, 'bmA parentId should be Folder B');",
        @"browser.test.assertEq(1, movedBm1.index, 'bmA should be at index 1 in Folder B');",

        @"let bmD = await browser.bookmarks.create({ title: 'Bookmark D', url: 'http://d.com', parentId: folderA.id });",
        @"let movedBm2 = await browser.bookmarks.move(bmC.id, { parentId: folderA.id, index: 2 });",
        @"browser.test.assertEq(bmC.id, movedBm2.id, 'Moved bookmark ID should match');",
        @"browser.test.assertEq(folderA.id, movedBm2.parentId, 'bmC parentId should be Folder A');",
        @"browser.test.assertEq(1, movedBm2.index, 'bmC should be at index 1 in Folder A');",

        @"let movedFolder = await browser.bookmarks.move(folderB.id, { index: 0 });",
        @"browser.test.assertEq(folderB.id, movedFolder.id, 'Moved folder ID should match');",
        @"browser.test.assertEq('other', movedFolder.parentId, 'Folder B should stay in its folder');",
        @"browser.test.assertEq(0, movedFolder.index, 'Folder B should be first in its folder');",
        @"let movedFolder2 = await browser.bookmarks.move(folderB.id, { parentId: folderA.id });",
        @"browser.test.assertEq(folderB.id, movedFolder2.id, 'Moved folder ID should match');",
        @"browser.test.assertEq(folderA.id, movedFolder2.parentId, 'Folder B should be a child of Folder A');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.move('nonexistent-id', { parentId: folderA.id }),",
        @"/bookmark not found/i,",
        @"'FAIL-1: Moving a non-existent bookmark should reject with the correct message');",

        @"browser.test.log('Testing move to a non-folder parent...');",
        @"await browser.test.assertRejects(",
        @"browser.bookmarks.move(bmB.id, { parentId: bmD.id }),",
        @"/the new parent is not a folder/i,",
        @"'FAIL-2: Moving to a non-folder parent should reject with the correct message');",

        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, Search)
{
    auto *script = @[
        @"let folderA = await browser.bookmarks.create({ id: 'folderA_id', title: 'Animals Folder' });",
        @"let bm1 = await browser.bookmarks.create({ id: 'bm1_id', parentId: folderA.id, title: 'Dog', url: 'http://animals.com/dog' });",
        @"let bm2 = await browser.bookmarks.create({ id: 'bm2_id', parentId: folderA.id, title: 'Cat', url: 'http://animals.com/cat' });",
        @"let bm3 = await browser.bookmarks.create({ id: 'bm3_id', parentId: folderA.id, title: 'Bird', url: 'http://birds.org/sparrow' });",

        @"let folderB = await browser.bookmarks.create({ id: 'folderB_id', title: 'Plants Folder' });",
        @"let bm4 = await browser.bookmarks.create({ id: 'bm4_id', parentId: folderB.id, title: 'Rose', url: 'http://flowers.com/rose' });",
        @"let bm5 = await browser.bookmarks.create({ id: 'bm5_id', parentId: folderB.id, title: 'Lily', url: 'http://flowers.com/lily' });",

        @"let bm6 = await browser.bookmarks.create({ id: 'bm6_id', title: 'Wild Animal', url: 'http://nature.com/wildlife' });",
        @"let bm7 = await browser.bookmarks.create({ id: 'bm7_id', title: 'My Favorite Dog Site', url: 'http://dogs.com/favorite' });",
        @"let bm8 = await browser.bookmarks.create({ id: 'bm8_id', title: 'A Bookmark', url: 'http://a.com/bookmark' });",
        @"let bm9 = await browser.bookmarks.create({ id: 'bm9_id', title: 'Another Bookmark', url: 'http://another.com/bookmark' });",
        @"let bm10 = await browser.bookmarks.create({ id: 'bm10_id', title: 'Special Folder' });",
        @"let bm11 = await browser.bookmarks.create({ id: 'bm11_id', title: 'Special Link', url: 'http://specialLink.com' });",

        @"let results1 = await browser.bookmarks.search('dog');",
        @"browser.test.assertEq(2, results1.length, 'S1: Should find 2 bookmarks for dog');",
        @"browser.test.assertEq(bm1.id, results1[0].id, 'S1: First result should be bm1');",
        @"browser.test.assertEq(bm7.id, results1[1].id, 'S1: Second result should be bm7');",

        @"let results2 = await browser.bookmarks.search('dog site');",
        @"browser.test.assertEq(1, results2.length, 'S2: Should find 1 bookmark for dog site');",
        @"browser.test.assertEq(bm7.id, results2[0].id, 'S2: Result should be bm7');",

        @"let results3 = await browser.bookmarks.search('Animals');",
        @"browser.test.assertEq(3, results3.length, 'S3: Should find 1 folder for Animals');",
        @"browser.test.assertEq(folderA.id, results3[0].id, 'S3: Result should be folderA');",
        @"browser.test.assertEq(bm1.id, results3[1].id, 'S3: Result should be bm1 since animals appears in the url');",
        @"browser.test.assertEq(bm2.id, results3[2].id, 'S3: Result should be bm2 since animals appears in the url and is after bm1');",

        @"let results4 = await browser.bookmarks.search('nonexistent');",
        @"browser.test.assertEq(0, results4.length, 'S4: Should find 0 bookmarks for nonexistent');",

        @"let results5 = await browser.bookmarks.search({ query: 'cat' });",
        @"browser.test.assertEq(1, results5.length, 'S5: Should find 1 bookmark for query cat');",
        @"browser.test.assertEq(bm2.id, results5[0].id, 'S5: Result should be bm2');",

        @"let results6 = await browser.bookmarks.search({ title: 'Bird' });",
        @"browser.test.assertEq(1, results6.length, 'S6: Should find 1 bookmark for title Bird');",
        @"browser.test.assertEq(bm3.id, results6[0].id, 'S6: Result should be bm3');",
        @"let results6_partial = await browser.bookmarks.search({ title: 'Bir' });",
        @"browser.test.assertEq(0, results6_partial.length, 'S6: Should find 0 bookmarks for partial title Bir');",
        @"let results6_case = await browser.bookmarks.search({ title: 'bird' });",
        @"browser.test.assertEq(0, results6_case.length, 'S6: Should find 0 bookmarks for case-sensitive title bird');",

        @"let results7 = await browser.bookmarks.search({ url: 'flowers.com' });",
        @"browser.test.assertEq(0, results7.length, 'S7: Should find 2 bookmarks for url flowers.com');",
        @"let results8 = await browser.bookmarks.search('flowers.com');",
        @"browser.test.assertEq(2, results8.length, 'S8: Should find 2 bookmarks for url flowers.com');",
        @"browser.test.assertEq(bm4.id, results8[0].id, 'S8: First result should be bm4');",
        @"browser.test.assertEq(bm5.id, results8[1].id, 'S8: Second result should be bm5');",

        @"let results9 = await browser.bookmarks.search({ title: 'Dog', url: 'http://animals.com/dog' });",
        @"browser.test.assertEq(1, results9.length, 'S9: Should find 1 bookmark for title Dog and url animals.com');",
        @"browser.test.assertEq(bm1.id, results9[0].id, 'S9: Result should be bm1');",

        @"let results9p2 = await browser.bookmarks.search({ title: 'Dog', url: 'https://animals.com/dog' });",
        @"browser.test.assertEq(0, results9p2.length, 'S9: Should find 0 bookmarks with https');",

        @"let results10 = await browser.bookmarks.search({ query: 'nature', title: 'Wild Animal' });",
        @"browser.test.assertEq(1, results10.length, 'S10: Should find 1 bookmark for query animal and title Wild Animal');",
        @"browser.test.assertEq(bm6.id, results10[0].id, 'S10: Result should be bm6');",

        @"let results11 = await browser.bookmarks.search({});",
        @"browser.test.assertEq(13, results11.length, 'S11: Should return all 10 items for empty query');",

        @"let results12 = await browser.bookmarks.search({ query: 'bookmark A' });",
        @"browser.test.assertEq(2, results12.length, 'S12: Should find 2 bookmarks for query bookmark a');",
        @"browser.test.assertEq(bm8.id, results12[0].id, 'S12: First result should be bm8');",
        @"browser.test.assertEq(bm9.id, results12[1].id, 'S12: Second result should be bm9');",

        @"let results13 = await browser.bookmarks.search({ query: 'Special' });",
        @"browser.test.assertEq(2, results13.length, 'S13: Should find 2 bookmarks for query Speical');",
        @"browser.test.assertEq(bm10.id, results13[0].id, 'S13: First result should be bm10');",
        @"browser.test.assertEq(bm11.id, results13[1].id, 'S13: Second result should be bm11');",
        @"let results14 = await browser.bookmarks.search({ query: 'Special', url: 'http://specialLink.com' });",
        @"browser.test.assertEq(1, results14.length, 'S14: Should find 1 bookmarks for query Speical');",
        @"browser.test.assertEq(bm11.id, results14[0].id, 'S14: First result should be bm11');",

        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, RootNodeIdentifier)
{
    auto *script = @[
        @"browser.test.assertEq('0', browser.bookmarks.ROOT_NODE_ID, 'The root identifier should be 0')",
        @"let topLevel = await browser.bookmarks.getChildren(browser.bookmarks.ROOT_NODE_ID)",
        @"browser.test.assertEq(1, topLevel.length, 'The root should have the top-level node')",
        @"browser.test.assertEq('Other', topLevel[0].title, 'The top-level node should be the default folder')",
        @"browser.test.assertEq(browser.bookmarks.ROOT_NODE_ID, topLevel[0].parentId, 'Top-level nodes should have the root as their parent')",
        @"let roots = await browser.bookmarks.get(browser.bookmarks.ROOT_NODE_ID)",
        @"browser.test.assertEq(1, roots.length, 'The root can be requested')",
        @"browser.test.assertEq(browser.bookmarks.ROOT_NODE_ID, roots[0].id, 'The root should have the root identifier')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, UnknownBookmarkIdentifier)
{
    auto *script = @[
        @"await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.getSubTree('missing'),",
        @"/bookmark not found/i,",
        @"'getSubTree with an unknown identifier should fail');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.getChildren('missing'),",
        @"/bookmark not found/i,",
        @"'getChildren with an unknown identifier should fail');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.get('missing'),",
        @"/bookmark not found/i,",
        @"'get with an unknown identifier should fail');",

        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, FolderTypeManagedAndSynced)
{
    auto *script = @[
        @"let tree = await browser.bookmarks.getTree()",
        @"let nodes = tree[0].children",
        @"browser.test.assertEq(4, nodes.length, 'The root should have four top-level nodes')",
        @"browser.test.assertEq('bookmarks-bar', nodes[0].folderType, 'The bookmarks bar should report its folder type')",
        @"browser.test.assertTrue(nodes[0].unmodifiable === undefined, 'The bookmarks bar should be modifiable')",
        @"browser.test.assertEq(true, nodes[0].syncing, 'The bookmarks bar should be syncing')",
        @"browser.test.assertEq('managed', nodes[1].folderType, 'The managed folder should report its folder type')",
        @"browser.test.assertEq('managed', nodes[1].unmodifiable, 'The managed folder should be unmodifiable')",
        @"browser.test.assertEq(false, nodes[1].syncing, 'The managed folder should never be syncing')",
        @"browser.test.assertEq('managed', nodes[1].children[0].unmodifiable, 'A managed bookmark should be unmodifiable')",
        @"browser.test.assertEq(false, nodes[1].children[0].syncing, 'A managed bookmark should never be syncing, even if the delegate says so')",
        @"browser.test.assertTrue(nodes[1].children[0].folderType === undefined, 'A bookmark should not have a folder type')",
        @"browser.test.assertEq('other', nodes[2].folderType, 'The other folder should report its folder type')",
        @"browser.test.assertEq(false, nodes[2].syncing, 'A node that does not say should not be syncing')",
        @"browser.test.assertTrue(nodes[3].folderType === undefined, 'An ordinary folder should not have a folder type')",
        @"let children = await browser.bookmarks.getChildren('managed')",
        @"browser.test.assertEq('managed', children[0].unmodifiable, 'getChildren should report unmodifiable')",
        @"let results = await browser.bookmarks.search({ title: 'Policy' })",
        @"browser.test.assertEq(1, results.length, 'The search should find the managed bookmark')",
        @"let folderResults = await browser.bookmarks.search('Favorites')",
        @"browser.test.assertEq(0, folderResults.length, 'The search should not return special folders')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    manager.get().internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
        auto *bar = @{ @"id": @"bar", @"title": @"Favorites", @"folderType": @"bookmarks-bar", @"syncing": @YES, @"children": @[ ] };
        auto *managedBookmark = @{ @"id": @"managed-bookmark", @"title": @"Policy", @"url": @"https://webkit.org/", @"managed": @YES, @"syncing": @YES };
        auto *managed = @{ @"id": @"managed", @"title": @"Managed", @"folderType": @"managed", @"managed": @YES, @"children": @[ managedBookmark ] };
        auto *other = @{ @"id": @"other", @"title": @"Other", @"folderType": @"other", @"children": @[ ] };
        auto *ordinary = @{ @"id": @"ordinary", @"title": @"Reading", @"children": @[ ] };

        auto *nodes = [NSMutableArray array];
        for (NSDictionary *dictionary in @[ bar, managed, other, ordinary ])
            [nodes addObject:adoptNS([[TestBookmarkNode alloc] initWithDictionary:dictionary]).get()];

        completionHandler(nodes, nil);
    };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, CreateUsesTheDefaultFolderAndIndex)
{
    auto *script = @[
        @"let first = await browser.bookmarks.create({ title: 'First', url: 'https://webkit.org/' })",
        @"browser.test.assertEq('other', first.parentId, 'A bookmark without a parent should go in the default folder')",
        @"browser.test.assertEq(0, first.index, 'The first bookmark should have index 0')",
        @"let second = await browser.bookmarks.create({ title: 'Second', url: 'https://webkit.org/blog/' })",
        @"browser.test.assertEq(1, second.index, 'A bookmark without an index should be added at the end')",
        @"let inserted = await browser.bookmarks.create({ title: 'Inserted', url: 'https://webkit.org/status/', index: 1 })",
        @"browser.test.assertEq(1, inserted.index, 'A bookmark with an index should be inserted there')",
        @"let children = await browser.bookmarks.getChildren('other')",
        @"browser.test.assertEq('First,Inserted,Second', children.map(child => child.title).join(','), 'The children should be in order')",

        @"browser.test.assertThrows(() => browser.bookmarks.create({ title: 'Negative', url: 'https://webkit.org/', index: -1 }), /The 'index' value is invalid, because it must not be negative/i)",
        @"await browser.test.assertRejects(",
        @"browser.bookmarks.create({ title: 'Too Far', url: 'https://webkit.org/', index: 4 }),",
        @"/index out of bounds/i,",
        @"'An index past the end should fail');",

        @"browser.test.assertThrows(() => browser.bookmarks.create({ title: 'Invalid', url: 'not a url' }), /The 'url' value is invalid, because it must be a valid URL/i)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, CreateWithoutDefaultFolderFails)
{
    auto *script = @[
        @"await browser.test.assertRejects(",
        @"browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' }),",
        @"/there is no default folder for new bookmarks/i,",
        @"'A bookmark without a parent needs a default folder');",
        @"let folder = await browser.bookmarks.create({ title: 'Folder', parentId: 'ordinary' })",
        @"browser.test.assertEq('ordinary', folder.parentId, 'A bookmark with a parent should not need a default folder')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    [[bookmarkStore topLevelBookmarks] removeAllObjects];
    [[bookmarkStore topLevelBookmarks] addObject:[@{ @"id": @"ordinary", @"title": @"Ordinary", @"type": @"folder", @"children": [NSMutableArray array] } mutableCopy]];

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, MutationsRespectTheRootAndManagedBookmarks)
{
    auto *script = @[
        @"let bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",

        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'In Root', parentId: browser.bookmarks.ROOT_NODE_ID }), /the bookmark root cannot be modified/i, 'create in the root')",
        @"await browser.test.assertRejects(browser.bookmarks.update(browser.bookmarks.ROOT_NODE_ID, { title: 'Renamed' }), /the bookmark root cannot be modified/i, 'update the root')",
        @"await browser.test.assertRejects(browser.bookmarks.move(bookmark.id, { parentId: browser.bookmarks.ROOT_NODE_ID }), /the bookmark root cannot be modified/i, 'move into the root')",
        @"await browser.test.assertRejects(browser.bookmarks.move(browser.bookmarks.ROOT_NODE_ID, { parentId: 'other' }), /the bookmark root cannot be modified/i, 'move the root')",
        @"await browser.test.assertRejects(browser.bookmarks.remove(browser.bookmarks.ROOT_NODE_ID), /the bookmark root cannot be modified/i, 'remove the root')",
        @"await browser.test.assertRejects(browser.bookmarks.removeTree(browser.bookmarks.ROOT_NODE_ID), /the bookmark root cannot be modified/i, 'remove the root tree')",

        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'In Managed', parentId: 'managed' }), /managed bookmarks cannot be modified/i, 'create in a managed folder')",
        @"await browser.test.assertRejects(browser.bookmarks.update('managed-bookmark', { title: 'Renamed' }), /managed bookmarks cannot be modified/i, 'update a managed bookmark')",
        @"await browser.test.assertRejects(browser.bookmarks.move(bookmark.id, { parentId: 'managed' }), /managed bookmarks cannot be modified/i, 'move into a managed folder')",
        @"await browser.test.assertRejects(browser.bookmarks.move('managed-bookmark', { parentId: 'other' }), /managed bookmarks cannot be modified/i, 'move a managed bookmark')",
        @"await browser.test.assertRejects(browser.bookmarks.remove('managed-bookmark'), /managed bookmarks cannot be modified/i, 'remove a managed bookmark')",
        @"await browser.test.assertRejects(browser.bookmarks.removeTree('managed'), /managed bookmarks cannot be modified/i, 'remove a managed folder')",

        @"let remaining = await browser.bookmarks.getChildren('managed')",
        @"browser.test.assertEq(1, remaining.length, 'The managed bookmark should still be there')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    [[bookmarkStore topLevelBookmarks] addObject:[@{ @"id": @"managed", @"title": @"Managed", @"type": @"folder", @"folderType": @"managed", @"managed": @YES, @"children": [@[ [@{ @"id": @"managed-bookmark", @"title": @"Policy", @"url": @"https://webkit.org/", @"managed": @YES } mutableCopy] ] mutableCopy] } mutableCopy]];

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, MoveRejectsInvalidDestinations)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({ title: 'Folder' })",
        @"let child = await browser.bookmarks.create({ title: 'Child', parentId: folder.id })",
        @"let bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",

        @"await browser.test.assertRejects(browser.bookmarks.move(folder.id, { parentId: folder.id }), /a folder cannot be moved into itself or its descendants/i, 'into itself')",
        @"await browser.test.assertRejects(browser.bookmarks.move(folder.id, { parentId: child.id }), /a folder cannot be moved into itself or its descendants/i, 'into a descendant')",
        @"await browser.test.assertRejects(browser.bookmarks.move(child.id, { parentId: bookmark.id }), /the new parent is not a folder/i, 'into a bookmark')",
        @"await browser.test.assertRejects(browser.bookmarks.move(child.id, { parentId: 'missing' }), /parent bookmark not found/i, 'into a missing folder')",
        @"await browser.test.assertRejects(browser.bookmarks.move(child.id, { parentId: folder.id, index: 3 }), /index out of bounds/i, 'past the end')",
        @"browser.test.assertThrows(() => browser.bookmarks.move(child.id, { index: -1 }), /The 'index' value is invalid, because it must not be negative/i)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, RemoveRequiresAnEmptyFolderUnlessRecursive)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({ title: 'Folder' })",
        @"let child = await browser.bookmarks.create({ title: 'Child', parentId: folder.id, url: 'https://webkit.org/' })",
        @"await browser.test.assertRejects(browser.bookmarks.remove(folder.id), /the folder is not empty, use removeTree\\(\\) to remove it/i, 'remove a folder with children')",
        @"await browser.bookmarks.removeTree(child.id)",
        @"let empty = await browser.bookmarks.getChildren(folder.id)",
        @"browser.test.assertEq(0, empty.length, 'removeTree should remove a bookmark')",
        @"await browser.bookmarks.remove(folder.id)",
        @"await browser.test.assertRejects(browser.bookmarks.getChildren(folder.id), /bookmark not found/i, 'the folder should be gone')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, ChangesFailWhenTheNodeCannotDoThem)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'Child', parentId: 'minimal' }), /it is not implemented/i, 'create')",
        @"await browser.test.assertRejects(browser.bookmarks.update('minimal', { title: 'Renamed' }), /it is not implemented/i, 'update')",
        @"await browser.test.assertRejects(browser.bookmarks.move('minimal', { parentId: 'minimal' }), /a folder cannot be moved into itself or its descendants/i, 'move into itself')",
        @"await browser.test.assertRejects(browser.bookmarks.remove('minimal'), /it is not implemented/i, 'remove')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    manager.get().internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
        completionHandler(@[ adoptNS([[TestMinimalBookmarkNode alloc] init]).get() ], nil);
    };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, DelegateCompletesAfterReturning)
{
    auto *script = @[
        @"let created = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",
        @"browser.test.assertEq('WebKit', created.title, 'The created bookmark should be returned')",
        @"let updated = await browser.bookmarks.update(created.id, { title: 'Blog' })",
        @"browser.test.assertEq('Blog', updated.title, 'The updated bookmark should be returned')",
        @"let folder = await browser.bookmarks.create({ title: 'Folder' })",
        @"let moved = await browser.bookmarks.move(created.id, { parentId: folder.id })",
        @"browser.test.assertEq(folder.id, moved.parentId, 'The moved bookmark should be in the folder')",
        @"let tree = await browser.bookmarks.getTree()",
        @"browser.test.assertEq(1, tree[0].children.length, 'The root should have one child')",
        @"await browser.bookmarks.remove(created.id)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    [bookmarkStore setCompletesAsynchronously:YES];

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetTreeWithBookmarkMissingOptionalMethods)
{
    auto *script = @[
        @"let tree = await browser.bookmarks.getTree()",
        @"browser.test.assertEq(1, tree[0].children.length, 'The root should have one child')",
        @"let node = tree[0].children[0]",
        @"browser.test.assertEq('minimal', node.id, 'The identifier should come from the delegate')",
        @"browser.test.assertEq('', node.title, 'The title should default to empty')",
        @"browser.test.assertEq(0, node.index, 'The index should be its position')",
        @"browser.test.assertEq('0', node.parentId, 'The parent should be the root')",
        @"browser.test.assertEq('folder', node.type, 'A node without a URL should be a folder')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    manager.get().internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
        completionHandler(@[ adoptNS([[TestMinimalBookmarkNode alloc] init]).get() ], nil);
    };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, InvalidIdentifiersAndChanges)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.get(['1', '']), /The 'idOrIdList' value is invalid, because each item in the list must be a non-empty string/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.get(123), /The 'idOrIdList' value is invalid, because a string or an array of strings is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.update('', { title: 'WebKit' }), /The 'id' value is invalid, because it must be a non-empty string/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.update('1', { }), /The 'changes' value is invalid, because it must contain a title or a url/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.search(123), /The 'query' value is invalid, because a string or an object is expected/i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}


TEST_F(WKWebExtensionAPIBookmarks, ChangesReportTheErrorFromTheApp)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'Docs', parentId: 'reading' }), /The bookmark database is locked/i, 'create')",
        @"await browser.test.assertRejects(browser.bookmarks.update('webkit', { title: 'WebKit Home' }), /The bookmark database is locked/i, 'update title')",
        @"await browser.test.assertRejects(browser.bookmarks.update('webkit', { url: 'https://example.org/' }), /The bookmark database is locked/i, 'update url')",
        @"await browser.test.assertRejects(browser.bookmarks.move('webkit', { parentId: 'reading' }), /The bookmark database is locked/i, 'move')",
        @"await browser.test.assertRejects(browser.bookmarks.remove('webkit'), /The bookmark database is locked/i, 'remove')",
        @"await browser.test.assertRejects(browser.bookmarks.removeTree('reading'), /The bookmark database is locked/i, 'removeTree')",
        @"browser.test.notifyPass()",
    ];

    NSMutableArray *children = [bookmarkStore topLevelBookmarks][0][@"children"];
    [children addObject:[@{ @"id": @"webkit", @"title": @"WebKit", @"url": @"https://webkit.org/" } mutableCopy]];
    [children addObject:[@{ @"id": @"reading", @"title": @"Reading", @"type": @"folder", @"children": [NSMutableArray array] } mutableCopy]];
    [bookmarkStore setFailureMessage:@"The bookmark database is locked"];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, CreateWithoutABookmarkOrAnErrorFails)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' }), /an unknown error occurred/i)",
        @"browser.test.notifyPass()",
    ];

    [bookmarkStore setCreateReturnsNoBookmark:YES];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetTreeFailsWhenTheAppReportsAnError)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.getTree(), /The bookmarks could not be read/i)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    manager.get().internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
        completionHandler(nil, [NSError errorWithDomain:@"TestBookmarks" code:1 userInfo:@{ NSLocalizedDescriptionKey: @"The bookmarks could not be read" }]);
    };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetTreeFailsWithoutBookmarksOrAnError)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.getTree(), /an unknown error occurred/i)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    manager.get().internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
        completionHandler(nil, nil);
    };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksFailWhenTheControllerDoesNotProvideThem)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.getTree(), /it is not implemented/i, 'getTree')",
        @"await browser.test.assertRejects(browser.bookmarks.get('1'), /it is not implemented/i, 'get')",
        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' }), /it is not implemented/i, 'create')",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, CreateInsideABookmarkFails)
{
    auto *script = @[
        @"const bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",
        @"await browser.test.assertRejects(browser.bookmarks.create({ title: 'Docs', parentId: bookmark.id }), /the parent is not a folder/i)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, CreateRejectsAnUnknownType)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.create({ title: 'Divider', type: 'separator' }), /it must specify either 'bookmark' or 'folder'/i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarksManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, UpdateNeedsTheBookmarkToSupportTheChange)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.bookmarks.update('read-only', { title: 'WebKit Home' }), /it is not implemented for 'title'/i, 'title')",
        @"await browser.test.assertRejects(browser.bookmarks.update('read-only', { url: 'https://example.org/' }), /it is not implemented for 'url'/i, 'url')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    manager.get().internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<id<_WKWebExtensionBookmark>> *, NSError *)) {
        completionHandler(@[ adoptNS([[TestStaticBookmarkNode alloc] init]).get() ], nil);
    };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksWithoutAnIdentifierAreSkipped)
{
    auto *script = @[
        @"const children = await browser.bookmarks.getChildren('other')",
        @"browser.test.assertEq(1, children.length, 'Only the bookmark with an identifier should be listed')",
        @"browser.test.assertEq('webkit', children[0].id)",
        @"browser.test.notifyPass()",
    ];

    NSMutableArray *children = [bookmarkStore topLevelBookmarks][0][@"children"];
    [children addObject:[@{ @"id": @"", @"title": @"Nameless", @"url": @"https://webkit.org/blog/" } mutableCopy]];
    [children addObject:[@{ @"id": @"webkit", @"title": @"WebKit", @"url": @"https://webkit.org/" } mutableCopy]];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}


TEST_F(WKWebExtensionAPIBookmarks, UpdateKeepsTheURLForEmptyOrInvalidValues)
{
    auto *script = @[
        @"const bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",
        @"const unchanged = await browser.bookmarks.update(bookmark.id, { title: 'WebKit Home', url: '' })",
        @"browser.test.assertEq('WebKit Home', unchanged.title, 'The title should change')",
        @"browser.test.assertEq('https://webkit.org/', unchanged.url, 'An empty URL should leave the URL alone')",
        @"browser.test.assertThrows(() => browser.bookmarks.update(bookmark.id, { url: 'this is not a valid url' }), /it must be a valid URL/i)",
        @"const current = await browser.bookmarks.get(bookmark.id)",
        @"browser.test.assertEq('https://webkit.org/', current[0].url, 'An invalid URL should not change the URL')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, CreateWithAnEmptyURLCreatesAFolder)
{
    auto *script = @[
        @"const folder = await browser.bookmarks.create({ title: 'Reading', url: '' })",
        @"browser.test.assertEq('folder', folder.type, 'An empty URL should create a folder')",
        @"browser.test.assertEq(undefined, folder.url, 'A folder should not have a URL')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetReturnsBookmarksInTheRequestedOrder)
{
    auto *script = @[
        @"const first = await browser.bookmarks.create({ title: 'First', url: 'https://webkit.org/' })",
        @"const second = await browser.bookmarks.create({ title: 'Second', url: 'https://webkit.org/blog/' })",
        @"const nodes = await browser.bookmarks.get([second.id, first.id])",
        @"browser.test.assertEq(2, nodes.length)",
        @"browser.test.assertEq(second.id, nodes[0].id, 'The first result should be the first identifier')",
        @"browser.test.assertEq(first.id, nodes[1].id, 'The second result should be the second identifier')",
        @"await browser.test.assertRejects(browser.bookmarks.get([first.id, 'missing']), /bookmark not found/i, 'One unknown identifier should fail the call')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, GetChildrenAndGetSubTreeShapes)
{
    auto *script = @[
        @"const parent = await browser.bookmarks.create({ title: 'Parent' })",
        @"const child = await browser.bookmarks.create({ title: 'Child', parentId: parent.id })",
        @"const leaf = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/', parentId: child.id })",
        @"const empty = await browser.bookmarks.create({ title: 'Empty' })",
        @"const children = await browser.bookmarks.getChildren(parent.id)",
        @"browser.test.assertEq(1, children.length, 'Only the immediate children should be returned')",
        @"browser.test.assertEq(undefined, children[0].children, 'The children should not include their own children')",
        @"const [subTree] = await browser.bookmarks.getSubTree(parent.id)",
        @"browser.test.assertEq(leaf.id, subTree.children[0].children[0].id, 'The subtree should include every descendant')",
        @"const [emptySubTree] = await browser.bookmarks.getSubTree(empty.id)",
        @"browser.test.assertEq(0, emptySubTree.children.length, 'An empty folder should have an empty list of children')",
        @"const [leafSubTree] = await browser.bookmarks.getSubTree(leaf.id)",
        @"browser.test.assertEq(leaf.id, leafSubTree.id, 'The subtree of a bookmark should be the bookmark')",
        @"browser.test.assertEq(0, (await browser.bookmarks.getChildren(leaf.id)).length, 'A bookmark should have no children')",
        @"await browser.test.assertRejects(browser.bookmarks.getChildren('missing'), /bookmark not found/i, 'getChildren')",
        @"await browser.test.assertRejects(browser.bookmarks.getSubTree('missing'), /bookmark not found/i, 'getSubTree')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, RemovedBookmarksLeaveGetAndSearch)
{
    auto *script = @[
        @"const folder = await browser.bookmarks.create({ title: 'Reading' })",
        @"const nested = await browser.bookmarks.create({ title: 'Nested Folder', parentId: folder.id })",
        @"const inner = await browser.bookmarks.create({ title: 'Inner Link', url: 'https://webkit.org/blog/', parentId: nested.id })",
        @"const leaf = await browser.bookmarks.create({ title: 'Outer Link', url: 'https://webkit.org/' })",
        @"const empty = await browser.bookmarks.create({ title: 'Empty' })",
        @"await browser.bookmarks.remove(leaf.id)",
        @"browser.test.assertEq(0, (await browser.bookmarks.search('Outer Link')).length, 'A removed bookmark should not be found')",
        @"await browser.test.assertRejects(browser.bookmarks.get(leaf.id), /bookmark not found/i, 'A removed bookmark should not be returned')",
        @"await browser.bookmarks.remove(empty.id)",
        @"await browser.test.assertRejects(browser.bookmarks.get(empty.id), /bookmark not found/i, 'A removed empty folder should not be returned')",
        @"await browser.bookmarks.removeTree(folder.id)",
        @"await browser.test.assertRejects(browser.bookmarks.get(inner.id), /bookmark not found/i, 'A descendant should be removed with the folder')",
        @"browser.test.assertEq(0, (await browser.bookmarks.search('Inner Link')).length, 'A removed descendant should not be found')",
        @"const another = await browser.bookmarks.create({ title: 'Another Link', url: 'https://webkit.org/status/' })",
        @"await browser.bookmarks.removeTree(another.id)",
        @"await browser.test.assertRejects(browser.bookmarks.get(another.id), /bookmark not found/i, 'removeTree should remove a bookmark')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, MovingAFolderKeepsItsChildren)
{
    auto *script = @[
        @"const source = await browser.bookmarks.create({ title: 'Source' })",
        @"const destination = await browser.bookmarks.create({ title: 'Destination' })",
        @"const link = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/', parentId: source.id })",
        @"const moved = await browser.bookmarks.move(source.id, { parentId: destination.id })",
        @"browser.test.assertEq(destination.id, moved.parentId, 'The folder should have the new parent')",
        @"const children = await browser.bookmarks.getChildren(source.id)",
        @"browser.test.assertEq(1, children.length, 'The folder should keep its children')",
        @"browser.test.assertEq(link.id, children[0].id)",
        @"await browser.test.assertRejects(browser.bookmarks.move(link.id, { parentId: 'missing' }), /parent bookmark not found/i, 'A move to an unknown parent should fail')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}



TEST_F(WKWebExtensionAPIBookmarks, UpdateRepliesWithTheChangedBookmark)
{
    auto *script = @[
        @"const bookmark = await browser.bookmarks.create({ title: 'WebKit', url: 'https://webkit.org/' })",
        @"const renamed = await browser.bookmarks.update(bookmark.id, { title: 'WebKit Home' })",
        @"browser.test.assertEq('WebKit Home', renamed.title, 'The reply should have the new title')",
        @"const moved = await browser.bookmarks.update(bookmark.id, { url: 'https://webkit.org/blog/' })",
        @"browser.test.assertEq('https://webkit.org/blog/', moved.url, 'The reply should have the new URL')",
        @"browser.test.assertEq('WebKit Home', moved.title, 'The reply should keep the earlier title')",
        @"browser.test.notifyPass()",
    ];

    [bookmarkStore setNodesAreSnapshots:YES];

    auto manager = getManagerFor(script, bookmarksManifest);

    configureBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

}
#endif
