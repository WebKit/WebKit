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
#import <WebKit/WebBookmarkSeparator.h>
#import <WebKit/WebKitDebug.h>

@interface WebBookmarkGroup (WebForwardDeclarations)
- (void)_setTopBookmark:(WebBookmark *)newTopBookmark;
@end

@implementation WebBookmarkGroup

+ (WebBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file
{
    return [[[WebBookmarkGroup alloc] initWithFile:file] autorelease];
}

- (id)initWithFile: (NSString *)file
{
    if (![super init]) {
        return nil;
    }

    _bookmarksByID = [[NSMutableDictionary dictionary] retain];

    _file = [file retain];
    [self _setTopBookmark:nil];

    // read history from disk
    [self loadBookmarkGroup];

    return self;
}

- (void)dealloc
{
    [_topBookmark release];
    [_file release];
    [_bookmarksByID release];
    [super dealloc];
}

- (WebBookmark *)topBookmark
{
    return _topBookmark;
}

- (void)_sendChangeNotificationForBookmark:(WebBookmark *)bookmark
                           childrenChanged:(BOOL)flag
{
    NSDictionary *userInfo;

    WEBKIT_ASSERT (bookmark != nil);
    
    if (_loading) {
        return;
    }

    userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        bookmark, WebModifiedBookmarkKey,
        [NSNumber numberWithBool:flag], WebBookmarkChildrenChangedKey,
        nil];
    
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebBookmarkGroupChangedNotification
                      object:self
                    userInfo:userInfo];
}

- (void)_setTopBookmark:(WebBookmark *)newTopBookmark
{
    WEBKIT_ASSERT_VALID_ARG (newTopBookmark, newTopBookmark == nil ||
                             [newTopBookmark bookmarkType] == WebBookmarkTypeList);
    
    [_topBookmark _setGroup:nil];
    [_topBookmark autorelease];

    if (newTopBookmark) {
        _topBookmark = [newTopBookmark retain];
    } else {
        _topBookmark = [[WebBookmarkList alloc] initWithTitle:nil image:nil group:self];
    }

    [self _sendChangeNotificationForBookmark:_topBookmark childrenChanged:YES];
}

- (void)_bookmarkDidChange:(WebBookmark *)bookmark
{
    [self _sendChangeNotificationForBookmark:bookmark childrenChanged:NO];
}

- (void)_bookmarkChildrenDidChange:(WebBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark bookmarkType] == WebBookmarkTypeList);
    
    [self _sendChangeNotificationForBookmark:bookmark childrenChanged:YES];
}

- (void)_removedBookmark:(WebBookmark *)bookmark
{
    WEBKIT_ASSERT ([_bookmarksByID objectForKey:[bookmark identifier]] == bookmark);
    [_bookmarksByID removeObjectForKey:[bookmark identifier]];
}

- (void)_addedBookmark:(WebBookmark *)bookmark
{
    WEBKIT_ASSERT ([_bookmarksByID objectForKey:[bookmark identifier]] == nil);
    [_bookmarksByID setObject:bookmark forKey:[bookmark identifier]];
}

- (WebBookmark *)bookmarkForIdentifier:(NSString *)identifier
{
    return [_bookmarksByID objectForKey:identifier];
}

- (void)removeBookmark:(WebBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark group] == self);
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark parent] != nil || bookmark == _topBookmark);

    if (bookmark == _topBookmark) {
        [self _setTopBookmark:nil];
    } else {
        [[bookmark parent] removeChild:bookmark];
        [bookmark _setGroup:nil];
    }
}

- (WebBookmark *)addNewBookmarkToBookmark:(WebBookmark *)parent
                               withTitle:(NSString *)newTitle
                                   image:(NSImage *)newImage
                               URLString:(NSString *)newURLString
                                    type:(WebBookmarkType)bookmarkType
{
    return [self insertNewBookmarkAtIndex:[parent numberOfChildren]
                               ofBookmark:parent
                                withTitle:newTitle
                                    image:newImage
                                URLString:newURLString
                                     type:bookmarkType];
}

- (WebBookmark *)insertNewBookmarkAtIndex:(unsigned)index
                              ofBookmark:(WebBookmark *)parent
                               withTitle:(NSString *)newTitle
                                   image:(NSImage *)newImage
                               URLString:(NSString *)newURLString
                                    type:(WebBookmarkType)bookmarkType
{
    WebBookmark *bookmark;

    WEBKIT_ASSERT_VALID_ARG (parent, [parent group] == self);
    WEBKIT_ASSERT_VALID_ARG (parent, [parent bookmarkType] == WebBookmarkTypeList);
    WEBKIT_ASSERT_VALID_ARG (newURLString, bookmarkType == WebBookmarkTypeLeaf || (newURLString == nil));
    
    if (bookmarkType == WebBookmarkTypeLeaf) {
        bookmark = [[[WebBookmarkLeaf alloc] initWithURLString:newURLString
                                                        title:newTitle
                                                        image:newImage
                                                        group:self] autorelease];
    } else if (bookmarkType == WebBookmarkTypeSeparator) {
        bookmark = [[[WebBookmarkSeparator alloc] initWithGroup:self] autorelease];
    } else {
        WEBKIT_ASSERT (bookmarkType == WebBookmarkTypeList);
        bookmark = [[[WebBookmarkList alloc] initWithTitle:newTitle
                                                    image:newImage
                                                    group:self] autorelease];
    }

    [parent insertChild:bookmark atIndex:index];
    return bookmark;
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
        WEBKITDEBUG("couldn't load bookmarks; couldn't find or create directory to store it in\n");
        return NO;
    }

    dictionary = [NSDictionary dictionaryWithContentsOfFile: path];
    if (dictionary == nil) {
        if (![[NSFileManager defaultManager] fileExistsAtPath: path]) {
            WEBKITDEBUG("no bookmarks file found at %s\n",
                        DEBUG_OBJECT(path));
        } else {
            WEBKITDEBUG("attempt to read bookmarks from %s failed; perhaps contents are corrupted\n",
                        DEBUG_OBJECT(path));
        }
        return NO;
    }

    _loading = YES;
    newTopBookmark = [[[WebBookmarkList alloc] initFromDictionaryRepresentation:dictionary withGroup:self] autorelease];
    [self _setTopBookmark:newTopBookmark];
    _loading = NO;

    return YES;
}

- (BOOL)loadBookmarkGroup
{
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _loadBookmarkGroupGuts];

    if (result == YES) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "loading %d bookmarks from %s took %f seconds\n",
                          [[self topBookmark] _numberOfDescendants], DEBUG_OBJECT([self file]), duration);
    }

    return result;
}

- (BOOL)_saveBookmarkGroupGuts
{
    NSString *path;
    NSDictionary *dictionary;

    path = [self file];
    if (path == nil) {
        WEBKITDEBUG("couldn't save bookmarks; couldn't find or create directory to store it in\n");
        return NO;
    }

    dictionary = [[self topBookmark] dictionaryRepresentation];
    if (![dictionary writeToFile:path atomically:YES]) {
        WEBKITDEBUG("attempt to save %s to %s failed\n", DEBUG_OBJECT(dictionary), DEBUG_OBJECT(path));
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
    
    if (result == YES) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "saving %d bookmarks to %s took %f seconds\n",
                          [[self topBookmark] _numberOfDescendants], DEBUG_OBJECT([self file]), duration);
    }

    return result;
}

@end
