//
//  IFBookmarkGroup.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <WebKit/IFBookmark.h>

// notification sent when bookmarks are added/removed from group, or when bookmarks in group are modified
#define IFBookmarkGroupChangedNotification	@"IFBookmarkGroupChangedNotification"

// keys for userInfo for IFBookmarkGroupChangedNotification. These are always present.

// The lowest common ancestor of all the IFBookmark objects that changed.
#define IFModifiedBookmarkKey			@"IFModifiedBookmarkKey"

// An NSNumber object representing a boolean that distinguishes changes
// to the bookmark itself from changes to its children.
#define	IFBookmarkChildrenChangedKey		@"IFBookmarkChildrenChangedKey"

@interface IFBookmarkGroup : NSObject
{
    NSString *_file;
    IFBookmark *_topBookmark;
    NSMutableDictionary *_bookmarksByID;
    BOOL _loading;
}

+ (IFBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file;
- (id)initWithFile: (NSString *)file;

// examining contents
- (IFBookmark *)topBookmark;
- (IFBookmark *)bookmarkForIdentifier:(NSString *)identifier;

// modifying contents
- (void)removeBookmark:(IFBookmark *)bookmark;

- (IFBookmark *)insertNewBookmarkAtIndex:(unsigned)index
                              ofBookmark:(IFBookmark *)parent
                               withTitle:(NSString *)newTitle
                                   image:(NSImage *)newImage
                               URLString:(NSString *)newURLString
                                    type:(IFBookmarkType)bookmarkType;
- (IFBookmark *)addNewBookmarkToBookmark:(IFBookmark *)parent
                               withTitle:(NSString *)newTitle
                                   image:(NSImage *)newImage
                               URLString:(NSString *)newURLString
                                    type:(IFBookmarkType)bookmarkType;

// storing contents on disk

// The file path used for storing this IFBookmarkGroup, specified in -[IFBookmarkGroup initWithFile:] or +[IFBookmarkGroup bookmarkGroupWithFile:]
- (NSString *)file;

// Load bookmark group from file. This happens automatically at init time, and need not normally be called.
- (BOOL)loadBookmarkGroup;

// Save bookmark group to file. It is the client's responsibility to call this at appropriate times.
- (BOOL)saveBookmarkGroup;

@end
