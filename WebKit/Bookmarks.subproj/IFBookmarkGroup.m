//
//  IFBookmarkGroup.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkGroup.h>
#import <WebKit/IFBookmarkGroup_Private.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/IFBookmarkList.h>
#import <WebKit/IFBookmarkLeaf.h>
#import <WebKit/IFBookmarkSeparator.h>
#import <WebKit/WebKitDebug.h>

@interface IFBookmarkGroup (IFForwardDeclarations)
- (void)_setTopBookmark:(IFBookmark *)newTopBookmark;
@end

@implementation IFBookmarkGroup

+ (IFBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file
{
    return [[[IFBookmarkGroup alloc] initWithFile:file] autorelease];
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

- (IFBookmark *)topBookmark
{
    return _topBookmark;
}

- (void)_sendChangeNotificationForBookmark:(IFBookmark *)bookmark
                           childrenChanged:(BOOL)flag
{
    NSDictionary *userInfo;

    WEBKIT_ASSERT (bookmark != nil);
    
    if (_loading) {
        return;
    }

    userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        bookmark, IFModifiedBookmarkKey,
        [NSNumber numberWithBool:flag], IFBookmarkChildrenChangedKey,
        nil];
    
    [[NSNotificationCenter defaultCenter]
        postNotificationName:IFBookmarkGroupChangedNotification
                      object:self
                    userInfo:userInfo];
}

- (void)_setTopBookmark:(IFBookmark *)newTopBookmark
{
    WEBKIT_ASSERT_VALID_ARG (newTopBookmark, newTopBookmark == nil ||
                             [newTopBookmark bookmarkType] == IFBookmarkTypeList);
    
    [_topBookmark _setGroup:nil];
    [_topBookmark autorelease];

    if (newTopBookmark) {
        _topBookmark = [newTopBookmark retain];
    } else {
        _topBookmark = [[IFBookmarkList alloc] initWithTitle:nil image:nil group:self];
    }

    [self _sendChangeNotificationForBookmark:_topBookmark childrenChanged:YES];
}

- (void)_bookmarkDidChange:(IFBookmark *)bookmark
{
    [self _sendChangeNotificationForBookmark:bookmark childrenChanged:NO];
}

- (void)_bookmarkChildrenDidChange:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark bookmarkType] == IFBookmarkTypeList);
    
    [self _sendChangeNotificationForBookmark:bookmark childrenChanged:YES];
}

- (void)_removedBookmark:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT ([_bookmarksByID objectForKey:[bookmark identifier]] == bookmark);
    [_bookmarksByID removeObjectForKey:[bookmark identifier]];
}

- (void)_addedBookmark:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT ([_bookmarksByID objectForKey:[bookmark identifier]] == nil);
    [_bookmarksByID setObject:bookmark forKey:[bookmark identifier]];
}

- (IFBookmark *)bookmarkForIdentifier:(NSString *)identifier
{
    return [_bookmarksByID objectForKey:identifier];
}

- (void)removeBookmark:(IFBookmark *)bookmark
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

- (IFBookmark *)addNewBookmarkToBookmark:(IFBookmark *)parent
                               withTitle:(NSString *)newTitle
                                   image:(NSImage *)newImage
                               URLString:(NSString *)newURLString
                                    type:(IFBookmarkType)bookmarkType
{
    return [self insertNewBookmarkAtIndex:[parent numberOfChildren]
                               ofBookmark:parent
                                withTitle:newTitle
                                    image:newImage
                                URLString:newURLString
                                     type:bookmarkType];
}

- (IFBookmark *)insertNewBookmarkAtIndex:(unsigned)index
                              ofBookmark:(IFBookmark *)parent
                               withTitle:(NSString *)newTitle
                                   image:(NSImage *)newImage
                               URLString:(NSString *)newURLString
                                    type:(IFBookmarkType)bookmarkType
{
    IFBookmark *bookmark;

    WEBKIT_ASSERT_VALID_ARG (parent, [parent group] == self);
    WEBKIT_ASSERT_VALID_ARG (parent, [parent bookmarkType] == IFBookmarkTypeList);
    WEBKIT_ASSERT_VALID_ARG (newURLString, bookmarkType == IFBookmarkTypeLeaf || (newURLString == nil));
    
    if (bookmarkType == IFBookmarkTypeLeaf) {
        bookmark = [[[IFBookmarkLeaf alloc] initWithURLString:newURLString
                                                        title:newTitle
                                                        image:newImage
                                                        group:self] autorelease];
    } else if (bookmarkType == IFBookmarkTypeSeparator) {
        bookmark = [[[IFBookmarkSeparator alloc] initWithGroup:self] autorelease];
    } else {
        WEBKIT_ASSERT (bookmarkType == IFBookmarkTypeList);
        bookmark = [[[IFBookmarkList alloc] initWithTitle:newTitle
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
    IFBookmarkList *newTopBookmark;

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
    newTopBookmark = [[[IFBookmarkList alloc] initFromDictionaryRepresentation:dictionary withGroup:self] autorelease];
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
