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

- (id)copyWithZone:(NSZone *)zone
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (NSString *)title
{
    if ([self bookmarkType] != IFBookmarkTypeSeparator) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
    return nil;
}

- (void)setTitle:(NSString *)title
{
    if ([self bookmarkType] != IFBookmarkTypeSeparator) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (NSImage *)image
{
    if ([self bookmarkType] != IFBookmarkTypeSeparator) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
    return nil;
}

- (void)setImage:(NSImage *)image
{
    if ([self bookmarkType] != IFBookmarkTypeSeparator) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (IFBookmarkType)bookmarkType
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return IFBookmarkTypeLeaf;
}

- (NSString *)URLString
{
    return nil;
}

- (void)setURLString:(NSString *)URLString
{
    if ([self bookmarkType] == IFBookmarkTypeLeaf) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (NSArray *)children
{
    if ([self bookmarkType] == IFBookmarkTypeList) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
    return nil;
}

- (unsigned)numberOfChildren
{
    if ([self bookmarkType] == IFBookmarkTypeList) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
    return 0;
}

- (unsigned)_numberOfDescendants
{
    if ([self bookmarkType] == IFBookmarkTypeList) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
    return 0;
}

- (void)insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index
{
    if ([self bookmarkType] == IFBookmarkTypeList) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (void)removeChild:(IFBookmark *)bookmark
{
    if ([self bookmarkType] == IFBookmarkTypeList) {
        NSRequestConcreteImplementation(self, _cmd, [self class]);
    }
}

- (IFBookmark *)_parent
{
    return _parent;
}

- (void)_setParent:(IFBookmark *)parent
{
    [parent retain];
    [_parent release];
    _parent = parent;
}

- (IFBookmarkGroup *)_group
{
    return _group;
}

- (void)_setGroup:(IFBookmarkGroup *)group
{
    [group retain];
    [_group release];
    _group = group;
}

- (id)_initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (NSDictionary *)_dictionaryRepresentation
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}


@end
