//
//  IFBookmarkGroup.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class IFBookmark;

@interface IFBookmarkGroup : NSObject
{
    NSString *_file;
    IFBookmark *_topBookmark;
}

+ (IFBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file;
- (id)initWithFile: (NSString *)file;

// examining contents
- (IFBookmark *)topBookmark;

// modifying contents
- (void)insertBookmark:(IFBookmark *)bookmark
               atIndex:(unsigned)index
            ofBookmark:(IFBookmark *)parent;
- (void)removeBookmark:(IFBookmark *)bookmark;

- (void)insertNewBookmarkAtIndex:(unsigned)index
                      ofBookmark:(IFBookmark *)parent
                           title:(NSString *)newTitle
                           image:(NSString *)newImage
                             URL:(NSString *)newURLString
                          isLeaf:(BOOL)flag;
- (void)updateBookmark:(IFBookmark *)bookmark
                 title:(NSString *)newTitle
                 image:(NSString *)newImage
                   URL:(NSString *)newURLString;

// storing contents on disk

// The file path used for storing this IFBookmarkGroup, specified in -[IFBookmarkGroup initWithFile:] or +[IFBookmarkGroup bookmarkGroupWithFile:]
- (NSString *)file;

// Load bookmark group from file. This happens automatically at init time, and need not normally be called.
- (BOOL)loadBookmarkGroup;

// Save bookmark group to file. It is the client's responsibility to call this at appropriate times.
- (BOOL)saveBookmarkGroup;

@end
