//
//  WebBookmarkGroup.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <WebKit/WebBookmark.h>

// Notifications sent when bookmarks are added/removed from group, or when bookmarks in group are modified
extern NSString *WebBookmarksWereAddedNotification;
extern NSString *WebBookmarksWereRemovedNotification;
extern NSString *WebBookmarkWillChangeNotification;
extern NSString *WebBookmarkDidChangeNotification;
extern NSString *WebBookmarksWillBeReloadedNotification;
extern NSString *WebBookmarksWereReloadedNotification;

// keys for userInfo for the above notifications.

// The lowest common ancestor of all the WebBookmark objects that changed.  This is the
// parent for adds and removes.
extern NSString *WebModifiedBookmarkKey;
// For adds and removes, the children that were added.  Value is always an array.
extern NSString *WebBookmarkChildrenKey;

@interface WebBookmarkGroup : NSObject
{
    NSString *_tag;
    NSString *_file;
    WebBookmark *_topBookmark;
    NSMutableDictionary *_bookmarksByUUID;
    BOOL _loading;
}

+ (WebBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file;
- (id)initWithFile: (NSString *)file;

// examining contents
- (WebBookmark *)topBookmark;

// modifying contents
- (WebBookmark *)insertNewBookmarkAtIndex:(unsigned)index
                              ofBookmark:(WebBookmark *)parent
                               withTitle:(NSString *)newTitle
                               URLString:(NSString *)newURLString
                                    type:(WebBookmarkType)bookmarkType;
- (WebBookmark *)addNewBookmarkToBookmark:(WebBookmark *)parent
                               withTitle:(NSString *)newTitle
                               URLString:(NSString *)newURLString
                                    type:(WebBookmarkType)bookmarkType;

// storing contents on disk

// The file path used for storing this WebBookmarkGroup, specified in -[WebBookmarkGroup initWithFile:] or +[WebBookmarkGroup bookmarkGroupWithFile:]
- (NSString *)file;

// Load bookmark group from file. This happens automatically at init time, and need not normally be called.
- (BOOL)loadBookmarkGroup;

// Marker for bookmark group, read from file. This cannot be changed in the current implementation.
// It can be used to see whether one file represents the same WebBookmarkGroup as another. If no
// tag is stored in the file, returns nil.
- (NSString *)tag;

// Save bookmark group to file. It is the client's responsibility to call this at appropriate times.
- (BOOL)saveBookmarkGroup;

@end
