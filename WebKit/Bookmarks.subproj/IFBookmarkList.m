//
//  IFBookmarkList.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkList.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/WebKitDebug.h>

@implementation IFBookmarkList

- (id)initWithTitle:(NSString *)title
              image:(NSImage *)image
              group:(IFBookmarkGroup *)group
{
    WEBKIT_ASSERT_VALID_ARG (group, group != nil);
    
    [super init];

    _title = [[NSString stringWithString:title] retain];
    _image = [image retain];
    _list = [[NSMutableArray array] retain];
    [self _setGroup:group];
    
    return self;
}

- (void)dealloc
{
    [_title release];
    [_image release];
    [_list release];
    [super dealloc];
}

- (NSString *)title
{
    return _title;
}

- (void)_setTitle:(NSString *)title
{
    NSString *oldTitle;

    oldTitle = _title;
    _title = [[NSString stringWithString:title] retain];
    [oldTitle release];
}

- (NSImage *)image
{
    return _image;
}

- (void)_setImage:(NSImage *)image
{
    [image retain];
    [_image release];
    _image = image;
}

- (BOOL)isLeaf
{
    return NO;
}

- (NSArray *)children
{
    return [NSArray arrayWithArray:_list];
}

- (void)_insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index
{
    [_list insertObject:bookmark atIndex:index];
}

@end
