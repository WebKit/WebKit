/*
 *  IFBookmark_Private.h
 *  WebKit
 *
 *  Created by John Sullivan on Tue Apr 30 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/IFBookmark.h>

@interface IFBookmark(IFPrivate)

- (id)_initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group;
- (NSDictionary *)_dictionaryRepresentation;

- (IFBookmark *)_parent;
- (void)_setParent:(IFBookmark *)parent;

- (IFBookmarkGroup *)_group;
- (void)_setGroup:(IFBookmarkGroup *)group;

- (unsigned)_numberOfDescendants;

@end

