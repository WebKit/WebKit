/*
 *  WebBookmarkGroupPrivate.h
 *  WebKit
 *
 *  Created by John Sullivan on Thu May 2 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/WebBookmarkGroup.h>

@interface WebBookmarkGroup(WebPrivate)

- (void)_bookmarkWillChange:(WebBookmark *)bookmark;
- (void)_bookmarkDidChange:(WebBookmark *)bookmark;
- (void)_bookmarkChildren:(NSArray *)kids wereAddedToParent:(WebBookmark *)bookmark;
- (void)_bookmarkChildren:(NSArray *)kids wereRemovedToParent:(WebBookmark *)bookmark;

@end

