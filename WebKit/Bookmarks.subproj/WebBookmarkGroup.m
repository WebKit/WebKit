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
#import <WebKit/WebKitDebug.h>

@interface IFBookmarkGroup (IFForwardDeclarations)
- (void)_resetTopBookmark;
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

    _file = [file retain];
    [self _resetTopBookmark];

    // read history from disk
    [self loadBookmarkGroup];

    return self;
}

- (void)dealloc
{
    [_topBookmark release];
    [_file release];
    [super dealloc];
}

- (IFBookmark *)topBookmark
{
    return _topBookmark;
}

- (void)_sendBookmarkGroupChangedNotification
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName: IFBookmarkGroupChangedNotification
                      object: self];
}

- (void)_resetTopBookmark
{
    BOOL hadChildren;

    hadChildren = [_topBookmark numberOfChildren] > 0;
    
    // bail out early if nothing needs resetting
    if (!hadChildren && _topBookmark != nil) {
        return;
    }

    [_topBookmark _setGroup:nil];
    [_topBookmark autorelease];
    _topBookmark = [[[IFBookmarkList alloc] initWithTitle:nil image:nil group:self] retain];

    if (hadChildren) {
        [self _sendBookmarkGroupChangedNotification];
    }
}

- (void)_bookmarkDidChange:(IFBookmark *)bookmark
{
    // FIXME: send enough info that organizing window can know
    // to only update this item
    [self _sendBookmarkGroupChangedNotification];
}

- (void)_bookmarkChildrenDidChange:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, ![bookmark isLeaf]);
    
    // FIXME: send enough info that organizing window can know
    // to only update this folder deep
    [self _sendBookmarkGroupChangedNotification];
}

- (void)insertBookmark:(IFBookmark *)bookmark
               atIndex:(unsigned)index
            ofBookmark:(IFBookmark *)parent
{
    _logNotYetImplemented();
}

- (void)removeBookmark:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark _group] == self);
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark _parent] != nil || bookmark == _topBookmark);

    if (bookmark == _topBookmark) {
        [self _resetTopBookmark];
    } else {
        [[bookmark _parent] removeChild:bookmark];
        [bookmark _setGroup:nil];
    }
}

- (void)addNewBookmarkToBookmark:(IFBookmark *)parent
                       withTitle:(NSString *)newTitle
                           image:(NSImage *)newImage
                       URLString:(NSString *)newURLString
                          isLeaf:(BOOL)flag
{
    [self insertNewBookmarkAtIndex:[parent numberOfChildren]
                        ofBookmark:parent
                         withTitle:newTitle
                             image:newImage
                         URLString:newURLString
                            isLeaf:flag];
}

- (void)insertNewBookmarkAtIndex:(unsigned)index
                      ofBookmark:(IFBookmark *)parent
                       withTitle:(NSString *)newTitle
                           image:(NSImage *)newImage
                       URLString:(NSString *)newURLString
                          isLeaf:(BOOL)flag
{
    IFBookmark *bookmark;

    WEBKIT_ASSERT_VALID_ARG (parent, [parent _group] == self);
    WEBKIT_ASSERT_VALID_ARG (parent, ![parent isLeaf]);
    WEBKIT_ASSERT_VALID_ARG (newURLString, flag ? (newURLString != nil) : (newURLString == nil));

    if (flag) {
        bookmark = [[[IFBookmarkLeaf alloc] initWithURLString:newURLString
                                                        title:newTitle
                                                        image:newImage
                                                        group:self] autorelease];
    } else {
        bookmark = [[[IFBookmarkList alloc] initWithTitle:newTitle
                                                    image:newImage
                                                    group:self] autorelease];
    }

    [parent insertChild:bookmark atIndex:index];
}

- (NSString *)file
{
    return _file;
}

- (BOOL)loadBookmarkGroup
{
    _logNotYetImplemented();
    return NO;
}

- (BOOL)saveBookmarkGroup
{
    _logNotYetImplemented();
   return NO;
}

@end
