//
//  WebBookmarkGroup.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <WebKit/WebBookmark.h>

// notification sent when bookmarks are added/removed from group, or when bookmarks in group are modified
#define WebBookmarkGroupChangedNotification	@"WebBookmarkGroupChangedNotification"

// keys for userInfo for WebBookmarkGroupChangedNotification. These are always present.

// The lowest common ancestor of all the WebBookmark objects that changed.
#define WebModifiedBookmarkKey			@"WebModifiedBookmarkKey"

// An NSNumber object representing a boolean that distinguishes changes
// to the bookmark itself from changes to its children.
#define	WebBookmarkChildrenChangedKey		@"WebBookmarkChildrenChangedKey"

@interface WebBookmarkGroup : NSObject
{
    NSString *_file;
    WebBookmark *_topBookmark;
    NSMutableDictionary *_bookmarksByID;
    BOOL _loading;
}

+ (WebBookmarkGroup *)bookmarkGroupWithFile: (NSString *)file;
- (id)initWithFile: (NSString *)file;

// examining contents
- (WebBookmark *)topBookmark;

// modifying contents
- (void)removeBookmark:(WebBookmark *)bookmark;

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

// Save bookmark group to file. It is the client's responsibility to call this at appropriate times.
- (BOOL)saveBookmarkGroup;

@end
