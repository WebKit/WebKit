/*
 *  IFBookmarkGroup_Private.h
 *  WebKit
 *
 *  Created by John Sullivan on Thu May 2 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/IFBookmarkGroup.h>

@interface IFBookmarkGroup(IFPrivate)

- (void)_bookmarkDidChange:(IFBookmark *)bookmark;
- (void)_bookmarkChildrenDidChange:(IFBookmark *)bookmark;

@end

