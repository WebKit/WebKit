/*
 *  WebBookmarkPrivate.h
 *  WebKit
 *
 *  Created by John Sullivan on Tue Apr 30 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/WebBookmark.h>

#define WebBookmarkTypeKey		@"WebBookmarkType"
#define WebBookmarkTypeLeafValue	@"WebBookmarkTypeLeaf"
#define WebBookmarkTypeListValue	@"WebBookmarkTypeList"

@interface WebBookmark(WebPrivate)

- (void)_setParent:(WebBookmark *)parent;
- (void)_setGroup:(WebBookmarkGroup *)group;

- (unsigned)_numberOfDescendants;

@end

