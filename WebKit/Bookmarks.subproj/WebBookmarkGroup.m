//
//  IFBookmarkGroup.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkGroup.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/IFBookmarkList.h>
#import <WebKit/IFBookmarkLeaf.h>
#import <WebKit/WebKitDebug.h>

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
    _topBookmark = [[[IFBookmarkList alloc] initWithTitle:nil image:nil group:self] retain];

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

- (void)insertBookmark:(IFBookmark *)bookmark
               atIndex:(unsigned)index
            ofBookmark:(IFBookmark *)parent
{
    _logNotYetImplemented();
}

- (void)removeBookmark:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark group] == self);
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark parent] != nil);

    [[bookmark parent] _removeChild:bookmark];
    [bookmark _setGroup:nil];
    
    [self _sendBookmarkGroupChangedNotification];
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

    WEBKIT_ASSERT_VALID_ARG (parent, [parent group] == self);
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

    [parent _insertChild:bookmark atIndex:index];
    [self _sendBookmarkGroupChangedNotification];
}

- (void)updateBookmark:(IFBookmark *)bookmark
                 title:(NSString *)newTitle
                 image:(NSString *)newImage
             URLString:(NSString *)newURLString
{
    _logNotYetImplemented();
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
