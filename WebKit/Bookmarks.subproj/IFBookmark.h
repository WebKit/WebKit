//
//  IFBookmark.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class IFBookmarkGroup;

@interface IFBookmark : NSObject {
    IFBookmark *_parent;
    IFBookmarkGroup *_group;
}

- (NSString *)title;
- (NSImage *)image;

// Distinguishes a single bookmark from a list 
- (BOOL)isLeaf;

// String intended to represent URL for leaf bookmarks. May not be a valid URL string.
// This is nil if isLeaf returns NO.
- (NSString *)URLString;

// Array of child IFBookmarks. This is nil if isLeaf returns YES.
- (NSArray *)children;

// Number of children. This is 0 if isLeaf returns YES.
- (unsigned)numberOfChildren;

// The list of bookmarks containing this one.
- (IFBookmark *)parent;

// The IFBookmarkGroup containing this bookmark. To make changes to a bookmark,
// use methods on its group.
- (IFBookmarkGroup *)group;

@end
