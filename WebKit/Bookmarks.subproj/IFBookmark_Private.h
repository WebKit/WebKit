/*
 *  IFBookmark_Private.h
 *  WebKit
 *
 *  Created by John Sullivan on Tue Apr 30 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/IFBookmark.h>

#define IFBookmarkTypeKey		@"IFBookmarkType"
#define IFBookmarkTypeLeafValue		@"IFBookmarkTypeLeaf"
#define IFBookmarkTypeListValue		@"IFBookmarkTypeList"
#define IFBookmarkTypeSeparatorValue	@"IFBookmarkTypeSeparator"

@interface IFBookmark(IFPrivate)

- (id)_initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group;
- (NSDictionary *)_dictionaryRepresentation;

- (void)_setParent:(IFBookmark *)parent;
- (void)_setGroup:(IFBookmarkGroup *)group;

- (unsigned)_numberOfDescendants;

@end

