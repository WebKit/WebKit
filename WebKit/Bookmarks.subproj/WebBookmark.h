//
//  WebBookmark.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class WebBookmarkGroup;

typedef enum {
    WebBookmarkTypeLeaf,
    WebBookmarkTypeList,
    WebBookmarkTypeProxy,
} WebBookmarkType;

@interface WebBookmark : NSObject <NSCopying> {
    WebBookmark *_parent;
    WebBookmarkGroup *_group;
    NSString *_identifier;
}

+ (WebBookmark *)bookmarkFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group;
+ (WebBookmark *)bookmarkOfType:(WebBookmarkType)type;

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group;
- (NSDictionary *)dictionaryRepresentation;

- (NSString *)title;
- (void)setTitle:(NSString *)title;

- (NSImage *)icon;

// The type of bookmark
- (WebBookmarkType)bookmarkType;

// String intended to represent URL for leaf bookmarks. May not be a valid URL string.
// This is nil if bookmarkType is not WebBookmarkTypeLeaf.
- (NSString *)URLString;

// Sets the string intended to represent URL for leaf bookmarks. URLString need not be
// a valid URL string. Does nothing if bookmarkType is not WebBookmarkTypeLeaf.
- (void)setURLString:(NSString *)URLString;

// String for client use; it is nil unless specified with setIdentifier
- (NSString *)identifier;

// Sets a string that can be retrieved later with -identifier. Not used internally
// in any way; clients can use it as they see fit.
- (void)setIdentifier:(NSString *)identifier;

// Array of child WebBookmarks. Returns nil if bookmarkType is not WebBookmarkTypeList.
// This creates a copy of the internal data structure, and thus is safe to (for example),
// iterate through, removing items from their parent as you go.
- (NSArray *)children;

// Array of child WebBookmarks. Returns nil if bookmarkType is not WebBookmarkTypeList.
// This returns the internal data structure itself. This array should not be modified,
// and operations that alter the children of this bookmark will affect the contents of this array.
- (NSArray *)rawChildren;

// Number of children. Returns 0 if bookmarkType is not WebBookmarkTypeList.
- (unsigned)numberOfChildren;

// Insert a bookmark into the list. Does nothing if bookmarkType is not WebBookmarkTypeList.
- (void)insertChild:(WebBookmark *)bookmark atIndex:(unsigned)index;

// Remove a bookmark from the list. Does nothing if bookmarkType is not WebBookmarkTypeList.
- (void)removeChild:(WebBookmark *)bookmark;

// The parent of this bookmark, or nil if this is the top bookmark in a group
- (WebBookmark *)parent;

// The group that this bookmark belongs to.
- (WebBookmarkGroup *)group;

// Compare the content (all information other than parent/group) to another bookmark
- (BOOL)contentMatches:(WebBookmark *)otherBookmark;

@end
