//
//  IFBookmark.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class IFBookmarkGroup;

typedef enum {
    IFBookmarkTypeLeaf,
    IFBookmarkTypeList,
    IFBookmarkTypeSeparator,
} IFBookmarkType;

@interface IFBookmark : NSObject <NSCopying> {
    IFBookmark *_parent;
    IFBookmarkGroup *_group;
    NSString *_identifier;
}

- (NSString *)title;
- (void)setTitle:(NSString *)title;

- (NSImage *)image;
- (void)setImage:(NSImage *)image;

// The type of bookmark
- (IFBookmarkType)bookmarkType;

// String intended to represent URL for leaf bookmarks. May not be a valid URL string.
// This is nil if bookmarkType is not IFBookmarkTypeLeaf.
- (NSString *)URLString;

// Sets the string intended to represent URL for leaf bookmarks. URLString need not be
// a valid URL string. Does nothing if bookmarkType is not IFBookmarkTypeLeaf.
- (void)setURLString:(NSString *)URLString;

// Array of child IFBookmarks. Returns nil if bookmarkType is not IFBookmarkTypeList.
- (NSArray *)children;

// Number of children. Returns 0 if bookmarkType is not IFBookmarkTypeList.
- (unsigned)numberOfChildren;

// Insert a bookmark into the list. Does nothing if bookmarkType is not IFBookmarkTypeList.
- (void)insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index;

// Remove a bookmark from the list. Does nothing if bookmarkType is not IFBookmarkTypeList.
- (void)removeChild:(IFBookmark *)bookmark;

// The parent of this bookmark, or nil if this is the top bookmark in a group
- (IFBookmark *)parent;

// The group that this bookmark belongs to.
- (IFBookmarkGroup *)group;

// An NSString that can be used to uniquely identify this bookmark; use with +[IFBookmarkGroup bookmarkForIdentifier];
- (NSString *)identifier;


@end
