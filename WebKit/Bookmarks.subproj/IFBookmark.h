//
//  IFBookmark.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class IFBookmarkGroup;

@interface IFBookmark : NSObject <NSCopying> {
    IFBookmark *_parent;
    IFBookmarkGroup *_group;
}

- (NSString *)title;
- (void)setTitle:(NSString *)title;

- (NSImage *)image;
- (void)setImage:(NSImage *)image;

// Distinguishes a single bookmark from a list 
- (BOOL)isLeaf;

// String intended to represent URL for leaf bookmarks. May not be a valid URL string.
// This is nil if isLeaf returns NO.
- (NSString *)URLString;

// Sets the string intended to represent URL for leaf bookmarks. URLString need not be
// a valid URL string. Does nothing if isLeaf returns NO.
- (void)setURLString:(NSString *)URLString;

// Array of child IFBookmarks. Returns nil if isLeaf returns YES.
- (NSArray *)children;

// Number of children. Returns 0 if isLeaf returns YES.
- (unsigned)numberOfChildren;

// Insert a bookmark into the list. Does nothing if isLeaf returns YES.
- (void)insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index;

// Remove a bookmark from the list. Does nothing if isLeaf returns YES.
- (void)removeChild:(IFBookmark *)bookmark;

@end
