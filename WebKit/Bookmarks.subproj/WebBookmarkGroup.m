//
//  WebBookmarkGroup.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmarkGroup.h>
#import <WebKit/WebBookmarkGroupPrivate.h>
#import <WebKit/WebBookmarkPrivate.h>
#import <WebKit/WebBookmarkList.h>
#import <WebKit/WebBookmarkLeaf.h>
#import <WebKit/WebBookmarkProxy.h>
#import <WebKit/WebKitLogging.h>

NSString *WebBookmarksWereAddedNotification = @"WebBookmarksWereAddedNotification";
NSString *WebBookmarksWereRemovedNotification = @"WebBookmarksWereRemovedNotification";
NSString *WebBookmarkDidChangeNotification = @"WebBookmarkDidChangeNotification";
NSString *WebBookmarkWillChangeNotification = @"WebBookmarkWillChangeNotification";
NSString *WebBookmarksWillBeReloadedNotification = @"WebBookmarksWillBeReloadedNotification";
NSString *WebBookmarksWereReloadedNotification = @"WebBookmarksWereReloadedNotification";
NSString *WebModifiedBookmarkKey = @"WebModifiedBookmarkKey";
NSString *WebBookmarkChildrenKey = @"WebBookmarkChildrenKey";
NSString *TagKey = @"WebBookmarkGroupTag";

@interface WebBookmarkGroup (WebForwardDeclarations)
- (void)_setTopBookmark:(WebBookmark *)newTopBookmark;
@end

@implementation WebBookmarkGroup

+ (WebBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file
{
    return [[[WebBookmarkGroup alloc] initWithFile:file] autorelease];
}

- (id)init
{
    ERROR("[WebBookmarkGroup init] not supported, use initWithFile: instead");
    [self release];
    return nil;
}

- (id)initWithFile: (NSString *)file
{
    if (![super init]) {
        return nil;
    }

    _file = [file copy];
    [self _setTopBookmark:nil];
    _bookmarksByUUID = [[NSMutableDictionary alloc] init];

    // read history from disk
    [self loadBookmarkGroup];

    return self;
}

- (void)dealloc
{
    [_bookmarksByUUID release];
    [_file release];
    [_tag release];
    [_topBookmark release];
    [super dealloc];
}

- (WebBookmark *)bookmarkForUUID:(NSString *)UUID
{
    return [_bookmarksByUUID objectForKey:UUID];
}

- (void)_addBookmark:(WebBookmark *)bookmark
{
    if ([bookmark group] == self) {
        return;
    }
    
    ASSERT([bookmark group] == nil);

    if ([bookmark _hasUUID]) {
        NSString *UUID = [bookmark UUID];
        // Clear UUID on former owner of this UUID (if any) -- new copy gets to keep it
        // FIXME 3153832: this means copy/paste transfers the UUID to the new bookmark.
        [[_bookmarksByUUID objectForKey:UUID] setUUID:nil];
        [_bookmarksByUUID setObject:bookmark forKey:UUID];
    }

    [bookmark _setGroup:self];

    // Recurse with bookmark's children
    NSArray *rawChildren = [bookmark rawChildren];
    unsigned count = [rawChildren count];
    unsigned childIndex;
    for (childIndex = 0; childIndex < count; ++childIndex) {
        [self _addBookmark:[rawChildren objectAtIndex:childIndex]];
    }
}

- (void)_bookmark:(WebBookmark *)bookmark changedUUIDFrom:(NSString *)oldUUID to:(NSString *)newUUID
{
    ASSERT([bookmark group] == self);

    if (oldUUID != nil) {
        [_bookmarksByUUID removeObjectForKey:oldUUID];
    }

    if (newUUID != nil) {
        [_bookmarksByUUID setObject:bookmark forKey:newUUID];
    }
}

- (void)_removeBookmark:(WebBookmark *)bookmark
{
    ASSERT([bookmark group] == self);

    if ([bookmark _hasUUID]) {
        ASSERT([_bookmarksByUUID objectForKey:[bookmark UUID]] == bookmark);
        [_bookmarksByUUID removeObjectForKey:[bookmark UUID]];
    }

    [bookmark _setGroup:nil];

    // Recurse with bookmark's children
    NSArray *rawChildren = [bookmark rawChildren];
    unsigned count = [rawChildren count];
    unsigned childIndex;
    for (childIndex = 0; childIndex < count; ++childIndex) {
        [self _removeBookmark:[rawChildren objectAtIndex:childIndex]];
    }
}

- (NSString *)tag
{
    return _tag;
}

- (WebBookmark *)topBookmark
{
    return _topBookmark;
}

- (void)_sendNotification:(NSString *)name forBookmark:(WebBookmark *)bookmark children:(NSArray *)kids
{
    NSDictionary *userInfo;

    // Some notifications (e.g. WillBeReloaded/WereReloaded) have no bookmark parameter. But
    // if there's a kids parameter, there must be a bookmark parameter also.
    ASSERT(bookmark != nil || kids == nil);
    
    if (_loading) {
        return;
    }

    userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        bookmark, WebModifiedBookmarkKey,
        kids, WebBookmarkChildrenKey,   // kids==nil will conveniently terminate the arg list
        nil];
    
    [[NSNotificationCenter defaultCenter] postNotificationName:name object:self userInfo:userInfo];
}

- (void)_setTopBookmark:(WebBookmark *)newTopBookmark
{
    ASSERT_ARG(newTopBookmark, newTopBookmark == nil ||
                             [newTopBookmark bookmarkType] == WebBookmarkTypeList);
    
    [newTopBookmark retain];

    if (_topBookmark != nil) {
        [self _removeBookmark:_topBookmark];
    }
    [_topBookmark release];

    if (newTopBookmark) {
        _topBookmark = newTopBookmark;
    } else {
        _topBookmark = [[WebBookmarkList alloc] initWithTitle:nil group:self];
    }
}

- (void)_bookmarkWillChange:(WebBookmark *)bookmark
{
    [self _sendNotification:WebBookmarkWillChangeNotification forBookmark:bookmark children:nil];
}

- (void)_bookmarkDidChange:(WebBookmark *)bookmark
{
    [self _sendNotification:WebBookmarkDidChangeNotification forBookmark:bookmark children:nil];
}

- (void)_bookmarkChildren:(NSArray *)kids wereAddedToParent:(WebBookmark *)bookmark
{
    ASSERT_ARG(bookmark, [bookmark bookmarkType] == WebBookmarkTypeList);
    [self _sendNotification:WebBookmarksWereAddedNotification forBookmark:bookmark children:kids];
}

- (void)_bookmarkChildren:(NSArray *)kids wereRemovedFromParent:(WebBookmark *)bookmark
{
    ASSERT_ARG(bookmark, [bookmark bookmarkType] == WebBookmarkTypeList);
    [self _sendNotification:WebBookmarksWereRemovedNotification forBookmark:bookmark children:kids];
}

- (void)_bookmarksWillBeReloaded
{
    [self _sendNotification:WebBookmarksWillBeReloadedNotification forBookmark:nil children:nil];
}

- (void)_bookmarksWereReloaded
{
    [self _sendNotification:WebBookmarksWereReloadedNotification forBookmark:nil children:nil];
}

- (WebBookmark *)addNewBookmarkToBookmark:(WebBookmark *)parent
                               withTitle:(NSString *)newTitle
                               URLString:(NSString *)newURLString
                                    type:(WebBookmarkType)bookmarkType
{
    return [self insertNewBookmarkAtIndex:[parent numberOfChildren]
                               ofBookmark:parent
                                withTitle:newTitle
                                URLString:newURLString
                                     type:bookmarkType];
}

- (WebBookmark *)insertNewBookmarkAtIndex:(unsigned)index
                              ofBookmark:(WebBookmark *)parent
                               withTitle:(NSString *)newTitle
                               URLString:(NSString *)newURLString
                                    type:(WebBookmarkType)bookmarkType
{
    WebBookmark *bookmark = nil;

    ASSERT_ARG(parent, [parent group] == self);
    ASSERT_ARG(parent, [parent bookmarkType] == WebBookmarkTypeList);
    ASSERT_ARG(newURLString, bookmarkType == WebBookmarkTypeLeaf || (newURLString == nil));

    switch (bookmarkType) {
        case WebBookmarkTypeLeaf:
            bookmark = [[WebBookmarkLeaf alloc] initWithURLString:newURLString
                                                            title:newTitle
                                                            group:self];
            break;
        case WebBookmarkTypeList:
            bookmark = [[WebBookmarkList alloc] initWithTitle:newTitle
                                                        group:self];
            break;
        case WebBookmarkTypeProxy:
            bookmark = [[WebBookmarkProxy alloc] initWithTitle:newTitle
                                                         group:self];
            break;
    }

    [parent insertChild:bookmark atIndex:index];
    return [bookmark autorelease];
}

- (NSString *)file
{
    return _file;
}

- (BOOL)_loadBookmarkGroupGuts
{
    NSString *path;
    NSDictionary *dictionary;
    WebBookmarkList *newTopBookmark;

    path = [self file];
    if (path == nil) {
        ERROR("couldn't load bookmarks; couldn't find or create directory to store it in");
        return NO;
    }

    dictionary = [NSDictionary dictionaryWithContentsOfFile: path];
    if (dictionary == nil) {
        if ([[NSFileManager defaultManager] fileExistsAtPath: path]) {
            ERROR("attempt to read bookmarks from %@ failed; perhaps contents are corrupted", path);
        }
        // else file doesn't exist, which is normal the first time
        return NO;
    }

    // If the bookmark group has a tag, it's in the root-level dictionary.
    NSString *tagFromFile = [dictionary objectForKey:TagKey];
    // we don't trust data read from disk, so double-check
    if ([tagFromFile isKindOfClass:[NSString class]]) {
        _tag = [tagFromFile retain];
    }

    [self _bookmarksWillBeReloaded];

    _loading = YES;
    newTopBookmark = [[WebBookmarkList alloc] initFromDictionaryRepresentation:dictionary withGroup:self];
    [self _setTopBookmark:newTopBookmark];
    [newTopBookmark release];
    _loading = NO;

    [self _bookmarksWereReloaded];

    return YES;
}

- (BOOL)loadBookmarkGroup
{
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _loadBookmarkGroupGuts];

    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "loading %d bookmarks from %@ took %f seconds",
            [[self topBookmark] _numberOfDescendants], [self file], duration);
    }

    return result;
}

- (BOOL)_saveBookmarkGroupGuts
{
    NSString *path;
    NSDictionary *dictionary;

    path = [self file];
    if (path == nil) {
        ERROR("couldn't save bookmarks; couldn't find or create directory to store it in");
        return NO;
    }

    dictionary = [[self topBookmark] dictionaryRepresentation];
    if (![dictionary writeToFile:path atomically:YES]) {
        ERROR("attempt to save bookmarks to %@ failed", path);
        return NO;
    }

    return YES;
}

- (BOOL)saveBookmarkGroup
{
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _saveBookmarkGroupGuts];
    
    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "saving %d bookmarks to %@ took %f seconds",
            [[self topBookmark] _numberOfDescendants], [self file], duration);
    }

    return result;
}

@end
