//
//  IFBookmark.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmark.h>
#import <WebKit/IFBookmarkGroup.h>

// to get NSRequestConcreteImplementation
#import <Foundation/NSPrivateDecls.h>

@implementation IFBookmark

- (void)dealloc
{
    [_parent release];
    [_group release];
    
    [super dealloc];
}

- (NSString *)title
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (void)_setTitle:(NSString *)title
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (NSImage *)image
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (void)_setImage:(NSImage *)image
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (BOOL)isLeaf
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return YES;
}

- (NSString *)URLString
{
    return nil;
}

- (void)_setURLString:(NSString *)URLString
{
    if ([self isLeaf]) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (NSArray *)children
{
    if (![self isLeaf]) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
    return nil;
}

- (void)_insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index
{
    if (![self isLeaf]) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (IFBookmark *)parent
{
    return _parent;
}

- (void)_setParent:(IFBookmark *)parent
{
    [parent retain];
    [_parent release];
    _parent = parent;
}

- (IFBookmarkGroup *)group
{
    return _group;
}

- (void)_setGroup:(IFBookmarkGroup *)group
{
    [group retain];
    [_group release];
    _group = group;
}

@end
