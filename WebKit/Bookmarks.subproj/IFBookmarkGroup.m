//
//  IFBookmarkGroup.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkGroup.h>
#import <WebKit/IFBookmark.h>
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

- (void)insertBookmark:(IFBookmark *)bookmark
               atIndex:(unsigned)index
            ofBookmark:(IFBookmark *)parent
{
    _logNotYetImplemented();
}

- (void)removeBookmark:(IFBookmark *)bookmark
{
    _logNotYetImplemented();
}

- (void)insertNewBookmarkAtIndex:(unsigned)index
                      ofBookmark:(IFBookmark *)parent
                           title:(NSString *)newTitle
                           image:(NSString *)newImage
                             URL:(NSString *)newURLString
                          isLeaf:(BOOL)flag
{
    _logNotYetImplemented();
}

- (void)updateBookmark:(IFBookmark *)bookmark
                 title:(NSString *)newTitle
                 image:(NSString *)newImage
                   URL:(NSString *)newURLString
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
