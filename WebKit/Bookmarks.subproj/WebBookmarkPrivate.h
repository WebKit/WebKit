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

- (void)_setTitle:(NSString *)title;
- (void)_setImage:(NSImage *)image;
- (void)_setURLString:(NSString *)URLString;
- (void)_setParent:(IFBookmark *)parent;
- (void)_setGroup:(IFBookmarkGroup *)group;
- (void)_insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index;
- (void)_removeChild:(IFBookmark *)bookmark;

@end

