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
#import "HTTPServer.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionPermissionPrivate.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebExtensionBookmarks.h>


@interface _MockBookmarkNode : NSObject <_WKWebExtensionBookmark>
- (instancetype)initWithDictionary:(NSDictionary *)dictionary;
@property (nonatomic, strong) NSMutableDictionary *dictionary;
@end

@implementation _MockBookmarkNode
- (instancetype)initWithDictionary:(NSDictionary *)dictionary
{
    self = [super init];
    if (self)
        _dictionary = [dictionary mutableCopy];
    return self;
}

- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"id"];
}

- (NSString *)parentIdentifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"parentId"];
}

- (NSString *)titleForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"title"];
}

- (NSString *)urlStringForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"url"];
}

- (NSInteger)indexForWebExtensionContext:(WKWebExtensionContext *)context
{
    return [_dictionary[@"index"] integerValue];
}

- (_WKWebExtensionBookmarkType)bookmarkTypeForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSString *typeString = self.dictionary[@"type"];
    if ([typeString isEqualToString:@"folder"])
        return _WKWebExtensionBookmarkTypeFolder;

    NSString *url = self.dictionary[@"url"];
    if (url && url.length > 0)
        return _WKWebExtensionBookmarkTypeBookmark;
    return _WKWebExtensionBookmarkTypeFolder;
}

- (NSArray<id<_WKWebExtensionBookmark>> *)childrenForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSArray *childDictionaries = _dictionary[@"children"];
    if (!childDictionaries)
        return nil;
    NSMutableArray<id<_WKWebExtensionBookmark>> *children = [NSMutableArray array];
    for (NSDictionary *childDict in childDictionaries)
        [children addObject:[[_MockBookmarkNode alloc] initWithDictionary:childDict]];
    return children;
}

- (NSDate *)dateAddedForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSNumber *dateValue = WebKit::objectForKey<NSNumber>(_dictionary, @"dateAdded");
    if (!dateValue)
        return nil;

    double millisecondsSinceEpoch = dateValue.doubleValue;
    NSTimeInterval secondsSinceEpoch = millisecondsSinceEpoch / 1000.0;

    return [NSDate dateWithTimeIntervalSince1970:secondsSinceEpoch];
}

@end


@interface TestBookmarksDelegate : NSObject <WKWebExtensionControllerDelegatePrivate>
@property (nonatomic, strong) NSMutableArray<NSMutableDictionary *> *mockBookmarks;
@property (nonatomic) NSInteger nextMockBookmarkId;
@end

static NSMutableDictionary *findParentInMockTree(NSMutableArray<NSMutableDictionary *> *tree, NSString *parentId)
{
    for (NSMutableDictionary *node in tree) {
        if ([node[@"id"] isEqualToString:parentId])
            return node;
        id children = node[@"children"];
        if (children && [children isKindOfClass:[NSMutableArray class]]) {
            NSMutableDictionary *found = findParentInMockTree(children, parentId);
            if (found)
                return found;
        }
    }
    return nil;
}

static NSMutableDictionary *findBookmarkAndParentArrayInMockTree(NSMutableArray *children, NSString *bookmarkId)
{
    if (!children.count)
        return nil;

    for (NSMutableDictionary *childDict in children) {
        if ([childDict[@"id"] isEqualToString:bookmarkId])
            return [@{ @"bookmark": childDict, @"parentChildren": children } mutableCopy];
        if ([childDict[@"type"] isEqualToString:@"folder"] && [childDict[@"children"] isKindOfClass:[NSMutableArray class]]) {
            NSMutableDictionary *found = findBookmarkAndParentArrayInMockTree(childDict[@"children"], bookmarkId);
            if (found)
                return found;
        }
    }
    return nil;
}

NSMutableDictionary* findBookmarkInMockTree(NSMutableArray *tree, NSString *bookmarkId)
{
    if (!tree || !bookmarkId)
        return nil;
    for (NSMutableDictionary *item in tree) {
        if ([item[@"id"] isEqualToString:bookmarkId])
            return item;
        if ([item[@"type"] isEqualToString:@"folder"] && item[@"children"]) {
            NSMutableDictionary *foundInChild = findBookmarkInMockTree(item[@"children"], bookmarkId);
            if (foundInChild)
                return foundInChild;
        }
    }
    return nil;
}


@implementation TestBookmarksDelegate
- (instancetype)init
{
    self = [super init];
    if (self) {
        _mockBookmarks = [NSMutableArray array];
        _nextMockBookmarkId = 100;
    }
    return self;
}

- (void)_webExtensionController:(WKWebExtensionController *)controller bookmarksForExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSArray<NSObject<_WKWebExtensionBookmark> *> *, NSError *))completionHandler
{
    NSMutableArray<NSObject<_WKWebExtensionBookmark> *> *results = [NSMutableArray array];
    for (NSDictionary *bookmarkDict in self.mockBookmarks)
        [results addObject:[[_MockBookmarkNode alloc] initWithDictionary:bookmarkDict]];
    completionHandler(results, nil);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller createBookmarkWithParentIdentifier:(NSString *)parentId index:(NSNumber *)index url:(NSString *)url title:(NSString *)title forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *))completionHandler
{
    NSMutableDictionary *newBookmark = [NSMutableDictionary dictionary];
    newBookmark[@"title"] = title;
    if (url)
        newBookmark[@"url"] = url;
    NSString *newId = [NSString stringWithFormat:@"%ld", (long)self.nextMockBookmarkId];
    _nextMockBookmarkId++;
    newBookmark[@"id"] = newId;

    if (!newBookmark[@"type"]) {
        NSString *url = newBookmark[@"url"];
        newBookmark[@"type"] = (url && url.length > 0) ? @"bookmark" : @"folder";
    }

    if (parentId && ![parentId isEqualToString:@"0"]) {
        NSMutableDictionary *parentDict = findParentInMockTree(self.mockBookmarks, parentId);
        if (parentDict) {
            NSMutableArray *children = [parentDict[@"children"] mutableCopy] ?: [NSMutableArray array];
            newBookmark[@"parentId"] = parentId;
            newBookmark[@"index"] = index ?: @(children.count);
            [children addObject:newBookmark];
            parentDict[@"children"] = children;

            completionHandler([[_MockBookmarkNode alloc] initWithDictionary:newBookmark], nil);
            return;
        }
    }

    newBookmark[@"parentId"] = @"0";
    newBookmark[@"index"] = index ?: @(self.mockBookmarks.count);
    [self.mockBookmarks addObject:newBookmark];

    completionHandler([[_MockBookmarkNode alloc] initWithDictionary:newBookmark], nil);
}
@end

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

static constexpr auto *bookmarkOnManifest = @{
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

    RetainPtr<NSMutableArray> uiProcessMockBookmarks;
    NSInteger nextMockBookmarkId;

    void SetUp() override
    {
        testing::Test::SetUp();
        uiProcessMockBookmarks = adoptNS([NSMutableArray new]);
        nextMockBookmarkId = 100;
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, bookmarkConfig);
    }

    void configureCreateBookmarkDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.createBookmarkWithParentIdentifier = ^(NSString *parentId, NSNumber *index, NSString *url, NSString *title, void (^completionHandler)(NSObject<_WKWebExtensionBookmark>*, NSError*)) {
            NSMutableDictionary *newBookmarkData = [NSMutableDictionary dictionary];
            newBookmarkData[@"title"] = title;
            if (url)
                newBookmarkData[@"url"] = url;
            double dateAddedInMilliseconds = NSDate.date.timeIntervalSince1970 * 1000.0;
            newBookmarkData[@"dateAdded"] = @(dateAddedInMilliseconds);

            if (!newBookmarkData[@"type"]) {
                NSString *url = newBookmarkData[@"url"];
                newBookmarkData[@"type"] = (url && url.length > 0) ? @"bookmark" : @"folder";
            }

            NSString *newId = [NSString stringWithFormat:@"%ld", (long)this->nextMockBookmarkId];
            this->nextMockBookmarkId++;
            newBookmarkData[@"id"] = newId;

            if (!newBookmarkData[@"type"]) {
                NSString *url = newBookmarkData[@"url"];
                newBookmarkData[@"type"] = (url && url.length > 0) ? @"bookmark" : @"folder";
            }

            newBookmarkData[@"parentId"] = parentId;
            if (parentId && ![parentId isEqualToString:@"0"]) {
                NSMutableDictionary *parentDict = findParentInMockTree(this->uiProcessMockBookmarks.get(), parentId);
                if (parentDict) {
                    NSMutableArray *children = [parentDict[@"children"] mutableCopy] ?: [NSMutableArray array];
                    newBookmarkData[@"parentId"] = parentId;
                    newBookmarkData[@"index"] = index ?: @(children.count);
                    [children addObject:newBookmarkData];
                    parentDict[@"children"] = children;
                    completionHandler(adoptNS([[_MockBookmarkNode alloc] initWithDictionary:newBookmarkData]).get(), nil);
                    return;
                }
            }

            newBookmarkData[@"index"] = index ?: @(this->uiProcessMockBookmarks.get().count);
            [this->uiProcessMockBookmarks addObject:newBookmarkData];
            completionHandler(adoptNS([[_MockBookmarkNode alloc] initWithDictionary:newBookmarkData]).get(), nil);
        };
    }

    void configureGetBookmarksDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<NSObject<_WKWebExtensionBookmark> *>*, NSError*)) {
            NSMutableArray<NSObject<_WKWebExtensionBookmark>*> *results = [NSMutableArray array];
            for (NSDictionary *dict in this->uiProcessMockBookmarks.get())
                [results addObject:[[_MockBookmarkNode alloc] initWithDictionary:dict]];
            completionHandler(results, nil);
        };
    }

    void configureRemoveBookmarksDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.removeBookmarkWithIdentifier = ^(NSString *bookmarkId, BOOL removeFolderWithChildren, void (^completionHandler)(NSError *)) {
            NSMutableDictionary *foundInfo = findBookmarkAndParentArrayInMockTree(uiProcessMockBookmarks.get(), bookmarkId);

            if (!foundInfo) {
                completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Bookmark with ID '%@' not found.", bookmarkId] }]);
                return;
            }

            NSMutableDictionary *foundBookmarkDict = foundInfo[@"bookmark"];
            NSMutableArray *parentChildrenArray = foundInfo[@"parentChildren"];
            NSString *foundBookmarkType = foundBookmarkDict[@"type"];

            if (!removeFolderWithChildren) {
                if ([foundBookmarkType isEqualToString:@"folder"]) {
                    NSArray *folderChildren = foundBookmarkDict[@"children"];

                    if (folderChildren.count) {
                        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Bookmark with ID '%@' is a non-empty folder and cannot be removed with bookmarks.remove(). Use bookmarks.removeTree().", bookmarkId] }]);
                        return;
                    }
                }
            } else {
                if ([foundBookmarkType isEqualToString:@"bookmark"]) {
                    completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Can't remove a non-folder item '%@' with bookmarks.removeTree(). Use bookmarks.remove().", bookmarkId] }]);
                    return;
                }
            }

            if (parentChildrenArray) {
                [parentChildrenArray removeObject:foundBookmarkDict];
                completionHandler(nil);
            } else
                completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: @"Failed to remove bookmark from top level (parent array not found)." }]);
        };
    }

    void configureUpdateBookmarksDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.updateBookmarkWithIdentifier = ^(NSString *bookmarkId, NSString *title, NSString *url, void (^completionHandler)(NSObject<_WKWebExtensionBookmark> *, NSError *)) {
            NSMutableDictionary *bookmarkToUpdate = findBookmarkInMockTree(uiProcessMockBookmarks.get(), bookmarkId);
            if (!bookmarkToUpdate) {
                completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Bookmark with ID '%@' not found.", bookmarkId] }]);
                return;
            }

            if (title)
                bookmarkToUpdate[@"title"] = title;

            if (url) {
                NSString *bookmarkType = bookmarkToUpdate[@"type"];
                if ([bookmarkType isEqualToString:@"folder"]) {
                    completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Cannot update URL for a %@ (ID: %@).", bookmarkType, bookmarkId] }]);
                    return;
                }
                bookmarkToUpdate[@"url"] = url;
            }

            _MockBookmarkNode *updatedMockNode = [[_MockBookmarkNode alloc] initWithDictionary:bookmarkToUpdate];
            completionHandler(updatedMockNode, nil);
        };
    }

    void configureMoveBookmarksDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.moveBookmarkWithIdentifier = ^(NSString *bookmarkId, NSString *toParentId, NSNumber *atIndex, void (^completionHandler)(NSObject<_WKWebExtensionBookmark> *, NSError *)) {
            NSMutableDictionary *foundInfo = findBookmarkAndParentArrayInMockTree(uiProcessMockBookmarks.get(), bookmarkId);
            if (!foundInfo) {
                completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Bookmark with ID '%@' not found for move.", bookmarkId] }]);
                return;
            }

            NSMutableDictionary *bookmarkToMove = foundInfo[@"bookmark"];
            NSMutableArray *oldParentChildrenArray = foundInfo[@"parentChildren"];

            NSUInteger oldIndex = [oldParentChildrenArray indexOfObject:bookmarkToMove];
            if (oldIndex == NSNotFound) {
                completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: @"Bookmark found but not in its reported parent array." }]);
                return;
            }

            NSMutableArray *newParentChildrenArray = nil;
            if (!toParentId)
                newParentChildrenArray = uiProcessMockBookmarks.get();
            else {
                NSMutableDictionary *newParent = findBookmarkInMockTree(uiProcessMockBookmarks.get(), toParentId);
                if (!newParent || ![newParent[@"type"] isEqualToString:@"folder"]) {
                    completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"New parent folder with ID '%@' not found or not a folder.", toParentId] }]);
                    return;
                }
                newParentChildrenArray = newParent[@"children"];
            }

            NSUInteger rawTargetIndex = atIndex ? atIndex.unsignedIntegerValue : newParentChildrenArray.count;
            [oldParentChildrenArray removeObjectAtIndex:oldIndex];

            if (oldParentChildrenArray == newParentChildrenArray && oldIndex < rawTargetIndex)
                rawTargetIndex--;

            NSUInteger finalTargetIndex = rawTargetIndex;
            if (finalTargetIndex > newParentChildrenArray.count)
                finalTargetIndex = newParentChildrenArray.count;

            bookmarkToMove[@"parentId"] = toParentId ?: @"0";
            NSUInteger targetIndex = atIndex ? atIndex.unsignedIntegerValue : newParentChildrenArray.count;
            if (targetIndex > newParentChildrenArray.count)
                targetIndex = newParentChildrenArray.count;

            [newParentChildrenArray insertObject:bookmarkToMove atIndex:targetIndex];
            bookmarkToMove[@"index"] = @(targetIndex);

            _MockBookmarkNode *movedMockNode = [[_MockBookmarkNode alloc] initWithDictionary:bookmarkToMove];
            completionHandler(movedMockNode, nil);
        };
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

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetRecent)
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
        @"browser.test.assertEq('Oldest Bookmark', recent2[2].title, 'Second result should be the middle bookmark')",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICreate)
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
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());

    [manager loadAndRun];

}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetTree)
{
    auto *script = @[
        @"let bookmark1 = await browser.bookmarks.create({type: 'bookmark', title: 'Top Bookmark 1', url: 'http://example.com/bm1'});",
        @"let folder1 = await browser.bookmarks.create({type: 'folder', title: 'Top Folder 1'});",
        @"let bookmark2 = await browser.bookmarks.create({title: 'Child Bookmark 2', url: 'http://example.com/bm2', parentId: folder1.id});",
        @"let bookmark3 = await browser.bookmarks.create({title: 'Top Bookmark 3', url: 'http://example.com/bm3'});",
        @"let root = await browser.bookmarks.getTree();",
        @"browser.test.assertTrue(Array.isArray(root), 'Root object should have a children array');",
        @"browser.test.assertTrue(root.length >= 1, 'Root should have at least one child (default folder)');",
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
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICreateAndGetTree)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({ title: 'Test Folder' });",
        @"browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');",
        @"browser.test.assertTrue(!!folder.id, 'Folder should have an ID');",
        @"let bookmark = await browser.bookmarks.create({ parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });",
        @"browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');",
        @"let rootNode = await browser.bookmarks.getTree();",
        @"browser.test.assertTrue(Array.isArray(rootNode), 'Root object should have a children array');",
        @"browser.test.assertEq(rootNode.length, 1);",
        @"browser.test.assertEq(rootNode[0].title, 'Test Folder');",
        @"browser.test.assertTrue(Array.isArray(rootNode[0].children), 'Folder should have a children array');",
        @"browser.test.assertEq(rootNode[0].children[0].title, 'WebKit.org', 'Child bookmark in tree should have correct title');",
        @"browser.test.assertEq(rootNode[0].children[0].url, 'https://webkit.org/', 'Child bookmark in tree should have correct URL');",
        @"let bookmark2 = await browser.bookmarks.create({ id: 'topLevelBookmark', title: 'Test Top Bookmark', url: 'https://coolbook.com/' });",
        @"browser.test.assertEq(bookmark2.title, 'Test Top Bookmark', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark2.url, 'https://coolbook.com/', 'Bookmark URL should be correct');",
        @"let updatedRootNode = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(updatedRootNode.length, 2);",
        @"browser.test.assertEq(updatedRootNode[0].title, 'Test Folder');",
        @"browser.test.assertEq(updatedRootNode[1].title, 'Test Top Bookmark');",
        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetSubTreeChildren)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({title: 'Test Folder' });",
        @"browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');",
        @"browser.test.assertTrue(!!folder.id, 'Folder should have an ID');",
        @"let bookmark = await browser.bookmarks.create({parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });",
        @"browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');",
        @"let folder2 = await browser.bookmarks.create({parentId: folder.id, title: 'Folder 2'});",
        @"browser.test.assertEq(folder2.title, 'Folder 2', 'Bookmark title should be correct');",
        @"browser.test.assertEq(folder2.parentId, folder.id, 'Bookmark URL should be correct');",
        @"let bookmark2 = await browser.bookmarks.create({parentId: folder2.id, title: 'Bookmark 2', url: 'https://bookmark2.org/' });",
        @"browser.test.log(`Starting bookmark test right before...: ${JSON.stringify(folder.id)}`);",
        @"let subtreeFolder1 = await browser.bookmarks.getSubTree(folder.id);",
        @"browser.test.assertTrue(Array.isArray(subtreeFolder1), 'subtreeFolder1 should be an array');",
        @"browser.test.assertEq(2, subtreeFolder1.length, 'subtreeFolder1 array should have 2 elements');",
        @"browser.test.assertEq('Folder 2', subtreeFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.assertEq('folder', subtreeFolder1[1].type, 'type should be folder');",
        @"browser.test.assertTrue(Array.isArray(subtreeFolder1[1].children), 'Folder 2 should have children');",
        @"browser.test.assertEq(1, subtreeFolder1[1].children.length, 'folder 2 should have 1 child');",
        @"browser.test.assertEq(bookmark2.title, subtreeFolder1[1].children[0].title, 'bookmark2 should be child of folder2');",
        @"browser.test.assertEq(bookmark.id, subtreeFolder1[0].id, 'Second child is bookmark');",
        @"let subtreeBookmark = await browser.bookmarks.getSubTree(bookmark.id);",
        @"browser.test.assertTrue(Array.isArray(subtreeBookmark), 'subtreeBookmark should be an array');",
        @"browser.test.assertEq(0, subtreeBookmark.length, 'subtreeBookmark array should have 1 element');",
        @"let childrenFolder1 = await browser.bookmarks.getChildren(folder.id);",
        @"browser.test.assertTrue(Array.isArray(childrenFolder1), 'childrenFolder1 should be an array');",
        @"browser.test.assertEq('Folder 2', childrenFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.log(`Starting bookmark test right before...: ${JSON.stringify(folder2.id)}`);",
        @"let getFolder1 = await browser.bookmarks.get([folder.id, folder2.id]);",
        @"browser.test.assertTrue(Array.isArray(getFolder1), 'getFolder1 should be an array');",
        @"browser.test.assertEq(2, getFolder1.length, 'getFolder1 array should have 2 elements');",
        @"browser.test.assertEq('Folder 2', getFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.assertEq('Test Folder', getFolder1[0].title, 'childs title should be Folder 2');",
        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIRemoveAndRemoveTree)
{
    auto script = @[
        @"let folder1 = await browser.bookmarks.create({ title: 'Folder One' });",
        @"let bookmarkA = await browser.bookmarks.create({ parentId: folder1.id, title: 'Bookmark A', url: 'http://example.com/a' });",
        @"let folder2 = await browser.bookmarks.create({ parentId: folder1.id, title: 'Folder Two' });",
        @"let bookmarkB = await browser.bookmarks.create({ parentId: folder2.id, title: 'Bookmark B', url: 'http://example.com/b' });",
        @"let topLevelBookmark = await browser.bookmarks.create({ title: 'Top Level Bookmark', url: 'http://example.com/top' });",
        @"let tree1 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(2, tree1[0].children.length, 'folder1 should have 2 children');",
        @"browser.test.assertEq('Top Level Bookmark', tree1[1].title, 'Second top-level is topBookmark');",
        @"browser.test.assertEq('Bookmark A', tree1[0].children[0].title, 'bookmarkA is child of folder1');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.removeTree(bookmarkA.id),",
        @"/Can't remove a non-folder item '.*' with bookmarks.removeTree\\(\\). Use bookmarks.remove\\(\\)./,",
        @"'FAIL-1: removeTree on a bookmark should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.remove(folder2.id),",
        @"/Bookmark with ID '.*' is a non-empty folder and cannot be removed with bookmarks.remove\\(\\). Use bookmarks.removeTree\\(\\)./,",
        @"'FAIL-2: remove on a non-empty folder should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.remove('nonexistent-id-123'),",
        @"/Bookmark with ID 'nonexistent-id-123' not found./,",
        @"'FAIL-3a: remove on a non-existent ID should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.removeTree('nonexistent-id-456'),",
        @"/Bookmark with ID 'nonexistent-id-456' not found./,",
        @"'FAIL-3b: removeTree on a non-existent ID should fail with the correct message');",

        @"await browser.bookmarks.remove(bookmarkA.id);",
        @"let tree2 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(1, tree2[0].children.length, 'folder1 should now have 1 child after removing bookmarkA');",
        @"browser.test.assertEq('Folder Two', tree2[0].children[0].title, 'folder2 should be the only child of folder1');",
        @"await browser.bookmarks.removeTree(folder2.id);",
        @"let tree3 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(0, tree3[0].children.length, 'folder1 should now have 0 children after removing folder2');",
        @"browser.test.assertEq(2, tree3.length, 'Tree should still have 2 top-level items (folder1 and topBookmark)');",
        @"browser.test.assertEq('Folder One', tree3[0].title, 'folder1 is still present');",
        @"browser.test.assertEq(0, tree3[0].children.length, 'folder1 is still present but has no children');",
        @"await browser.bookmarks.remove(folder1.id);",
        @"let tree4 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(1, tree4.length, 'the top level now ONLY has the top level bookmark');",
        @"browser.test.notifyPass();",
    ];

    auto resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    configureRemoveBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIUpdate)
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
        @"/Cannot update URL for a \\w+ \\(ID: .*\\)\\./,",
        @"'FAIL: Updating a folder URL should be rejected with the correct error message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.update('nonexistent-id', { title: 'New Title' }),",
        @"/Bookmark with ID 'nonexistent-id' not found\\./,",
        @"'FAIL: Updating a non-existent ID should be rejected with the correct error message');",

        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    configureUpdateBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIMove)
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
        @"let movedBm2 = await browser.bookmarks.move(bmC.id, { parentId: folderA.id, index: 1 });",
        @"browser.test.assertEq(bmC.id, movedBm2.id, 'Moved bookmark ID should match');",
        @"browser.test.assertEq(folderA.id, movedBm2.parentId, 'bmC parentId should be Folder A');",
        @"browser.test.assertEq(1, movedBm2.index, 'bmC should be at index 1 in Folder A');",

        @"let movedFolder = await browser.bookmarks.move(folderB.id, { index: 0 });",
        @"browser.test.assertEq(folderB.id, movedFolder.id, 'Moved folder ID should match');",
        @"browser.test.assertEq(0, movedFolder.index, 'Folder B parentId should be null (root)');",
        @"let movedFolder2 = await browser.bookmarks.move(folderB.id, { parentId: folderA.id });",
        @"browser.test.assertEq(folderB.id, movedFolder2.id, 'Moved folder ID should match');",
        @"browser.test.assertEq(folderA.id, movedFolder2.parentId, 'Folder B parentId should be null (root)');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.move('nonexistent-id', { parentId: folderA.id }),",
        @"/Bookmark with ID 'nonexistent-id' not found for move./,",
        @"'FAIL-1: Moving a non-existent bookmark should reject with the correct message');",

        @"browser.test.log('Testing move to a non-folder parent...');",
        @"await browser.test.assertRejects(",
        @"browser.bookmarks.move(bmB.id, { parentId: bmD.id }),",
        @"/New parent folder with ID '.*' not found or not a folder./,",
        @"'FAIL-2: Moving to a non-folder parent should reject with the correct message');",

        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    configureMoveBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPISearch)
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

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

}
#endif

