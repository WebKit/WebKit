//
//  WebBookmark.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmark.h>
#import <WebKit/WebBookmarkPrivate.h>
#import <WebKit/WebBookmarkGroup.h>
#import <WebKit/WebBookmarkGroupPrivate.h>
#import <WebKit/WebBookmarkLeaf.h>
#import <WebKit/WebBookmarkList.h>
#import <WebKit/WebBookmarkProxy.h>
#import <WebFoundation/WebAssertions.h>

// to get NSRequestConcreteImplementation
#import <Foundation/NSPrivateDecls.h>

@implementation WebBookmark

- (void)dealloc
{    
    [_group release];
    [_identifier release];
    
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (NSString *)title
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (void)setTitle:(NSString *)title
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (NSImage *)icon
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (WebBookmarkType)bookmarkType
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return WebBookmarkTypeLeaf;
}

- (NSString *)URLString
{
    return nil;
}

- (void)setURLString:(NSString *)URLString
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (NSString *)identifier
{
    return [[_identifier copy] autorelease];
}

- (void)setIdentifier:(NSString *)identifier
{
    if (identifier == _identifier) {
        return;
    }

    [_identifier release];
    _identifier = [identifier copy];
}

- (NSArray *)children
{
    return nil;
}

- (unsigned)numberOfChildren
{
    return 0;
}

- (unsigned)_numberOfDescendants
{
    return 0;
}

- (void)insertChild:(WebBookmark *)bookmark atIndex:(unsigned)index
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (void)removeChild:(WebBookmark *)bookmark
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (WebBookmark *)parent
{
    return _parent;
}

- (void)_setParent:(WebBookmark *)parent
{
    // Don't retain parent, to avoid circular ownership that prevents dealloc'ing
    // when a parent with children is removed from a group and has no other references.
    _parent = parent;
}

- (WebBookmarkGroup *)group
{
    return _group;
}

- (void)_setGroup:(WebBookmarkGroup *)group
{
    if (group == _group) {
        return;
    }

    [_group release];

    _group = [group retain];
}

+ (WebBookmark *)bookmarkOfType:(WebBookmarkType)type
{
    if (type == WebBookmarkTypeList) {
        return [[[WebBookmarkList alloc] init] autorelease];
    } else if (type == WebBookmarkTypeLeaf) {
        return [[[WebBookmarkLeaf alloc] init] autorelease];
    } else if (type == WebBookmarkTypeProxy) {
        return [[[WebBookmarkProxy alloc] init] autorelease];
    }

    return nil;
}


+ (WebBookmark *)bookmarkFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    NSString *typeString = [dict objectForKey:WebBookmarkTypeKey];
    
    if (![typeString isKindOfClass:[NSString class]]) {
        ERROR("bad dictionary");
        return nil;
    }
    
    Class class = nil;
    
    if ([typeString isEqualToString:WebBookmarkTypeListValue]) {
        class = [WebBookmarkList class];
    } else if ([typeString isEqualToString:WebBookmarkTypeLeafValue]) {
        class = [WebBookmarkLeaf class];
    } else if ([typeString isEqualToString:WebBookmarkTypeProxyValue]) {
        class = [WebBookmarkProxy class];
    }
    
    if (class) {
        return  [[[class alloc] initFromDictionaryRepresentation:dict
                                                       withGroup:group] autorelease];
    }

    return nil;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{    
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (BOOL)contentMatches:(WebBookmark *)otherBookmark
{
    WebBookmarkType bookmarkType = [self bookmarkType];
    if (bookmarkType != [otherBookmark bookmarkType]) {
        return NO;
    }

    if (![[self title] isEqualToString:[otherBookmark title]]) {
        return NO;
    }

    NSString *thisURLString = [self URLString];
    NSString *thatURLString = [otherBookmark URLString];
    // handle case where both are nil
    if (thisURLString != thatURLString && ![thisURLString isEqualToString:thatURLString]) {
        return NO;
    }

    NSString *thisIdentifier = [self identifier];
    NSString *thatIdentifier = [otherBookmark identifier];
    // handle case where both are nil
    if (thisIdentifier != thatIdentifier && ![thisIdentifier isEqualToString:thatIdentifier]) {
        return NO;
    }

    unsigned thisCount = [self numberOfChildren];
    if (thisCount != [otherBookmark numberOfChildren]) {
        return NO;
    }

    unsigned childIndex;
    for (childIndex = 0; childIndex < thisCount; ++childIndex) {
        NSArray *theseChildren = [self children];
        NSArray *thoseChildren = [otherBookmark children];
        if (![[theseChildren objectAtIndex:childIndex] contentMatches:[thoseChildren objectAtIndex:childIndex]]) {
            return NO;
        }
    }

    return YES;
}


@end
