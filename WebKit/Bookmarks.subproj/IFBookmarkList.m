//
//  IFBookmarkList.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkList.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/IFBookmarkGroup_Private.h>
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

- (void)setTitle:(NSString *)title
{
    if ([title isEqualToString:_title]) {
        return;
    }

    [_title release];
    _title = [[NSString stringWithString:title] retain];

    [[self _group] _bookmarkDidChange:self]; 
}

- (NSImage *)image
{
    return _image;
}

- (void)setImage:(NSImage *)image
{
    if ([image isEqual:_image]) {
        return;
    }
    
    [image retain];
    [_image release];
    _image = image;

    [[self _group] _bookmarkDidChange:self]; 
}

- (BOOL)isLeaf
{
    return NO;
}

- (NSArray *)children
{
    return [NSArray arrayWithArray:_list];
}

- (unsigned)numberOfChildren
{
    return [_list count];
}

- (void)removeChild:(IFBookmark *)bookmark
{
    WEBKIT_ASSERT_VALID_ARG (bookmark, [bookmark _parent] == self);
    [_list removeObject:bookmark];
    [bookmark _setParent:nil];

    [[self _group] _bookmarkChildrenDidChange:self]; 
}


- (void)insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index
{
    [_list insertObject:bookmark atIndex:index];
    [bookmark _setParent:self];
    
    [[self _group] _bookmarkChildrenDidChange:self]; 
}

@end
