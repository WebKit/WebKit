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
#define WebBookmarkTypeProxyValue	@"WebBookmarkTypeProxy"

#define WebBookmarkIdentifierKey	@"WebBookmarkIdentifier"
#define WebBookmarkUUIDKey		@"WebBookmarkUUID"

@interface WebBookmark(WebPrivate)

- (void)_setParent:(WebBookmark *)parent;
- (void)_setGroup:(WebBookmarkGroup *)group;

// Returns YES if UUID is non-nil; can't simply use -[WebBookmark UUID] because
// it will generate a UUID if there isn't one already.
- (BOOL)_hasUUID;


- (unsigned)_numberOfDescendants;

@end

