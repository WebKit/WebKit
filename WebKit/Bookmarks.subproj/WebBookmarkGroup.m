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
#import <WebKit/WebKitLogging.h>

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

    ASSERT(bookmark != nil);
    
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
    ASSERT_ARG(newTopBookmark, newTopBookmark == nil ||
                             [newTopBookmark bookmarkType] == WebBookmarkTypeList);
    
    [newTopBookmark retain];
    
    [_topBookmark _setGroup:nil];
    [_topBookmark release];

    if (newTopBookmark) {
        _topBookmark = newTopBookmark;
    } else {
        _topBookmark = [[WebBookmarkList alloc] initWithTitle:nil group:self];
    }

    [self _sendChangeNotificationForBookmark:_topBookmark childrenChanged:YES];
}

- (void)_bookmarkDidChange:(WebBookmark *)bookmark
{
    [self _sendChangeNotificationForBookmark:bookmark childrenChanged:NO];
}

- (void)_bookmarkChildrenDidChange:(WebBookmark *)bookmark
{
    ASSERT_ARG(bookmark, [bookmark bookmarkType] == WebBookmarkTypeList);
    
    [self _sendChangeNotificationForBookmark:bookmark childrenChanged:YES];
}

- (void)removeBookmark:(WebBookmark *)bookmark
{
    ASSERT_ARG(bookmark, [bookmark group] == self);
    ASSERT_ARG(bookmark, [bookmark parent] != nil || bookmark == _topBookmark);

    if (bookmark == _topBookmark) {
        [self _setTopBookmark:nil];
    } else {
        [[bookmark parent] removeChild:bookmark];
        [bookmark _setGroup:nil];
    }
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
    WebBookmark *bookmark;

    ASSERT_ARG(parent, [parent group] == self);
    ASSERT_ARG(parent, [parent bookmarkType] == WebBookmarkTypeList);
    ASSERT_ARG(newURLString, bookmarkType == WebBookmarkTypeLeaf || (newURLString == nil));
    
    if (bookmarkType == WebBookmarkTypeLeaf) {
        bookmark = [[WebBookmarkLeaf alloc] initWithURLString:newURLString
                                                        title:newTitle
                                                        group:self];
    } else {
        ASSERT(bookmarkType == WebBookmarkTypeList);
        bookmark = [[WebBookmarkList alloc] initWithTitle:newTitle
                                                    group:self];
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
        if (![[NSFileManager defaultManager] fileExistsAtPath: path]) {
            ERROR("no bookmarks file found at %@", path);
        } else {
            ERROR("attempt to read bookmarks from %@ failed; perhaps contents are corrupted", path);
        }
        return NO;
    }

    _loading = YES;
    newTopBookmark = [[WebBookmarkList alloc] initFromDictionaryRepresentation:dictionary withGroup:self];
    [self _setTopBookmark:newTopBookmark];
    [newTopBookmark release];
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
        ERROR("attempt to save %@ to %@ failed", dictionary, path);
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
        LOG(Timing, "saving %d bookmarks to %@ took %f seconds",
            [[self topBookmark] _numberOfDescendants], [self file], duration);
    }

    return result;
}

@end
