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
#import <WebKit/WebBookmarkSeparator.h>
#import <WebFoundation/WebAssertions.h>

// to get NSRequestConcreteImplementation
#import <Foundation/NSPrivateDecls.h>

@implementation WebBookmark

static unsigned _highestUsedID = 0;

+ (NSString *)_generateUniqueIdentifier
{
    return [[NSNumber numberWithInt:++_highestUsedID] stringValue];
}

- (id)init
{
    [super init];
    _identifier = [[WebBookmark _generateUniqueIdentifier] copy];
    return self;
}

- (void)dealloc
{
    ASSERT(_group == nil);

    [_identifier release];    
    [super dealloc];
}

- (NSString *)identifier
{
    ASSERT(_identifier != nil);
    return [[_identifier copy] autorelease];
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

    [_group _removedBookmark:self];
    [_group release];

    _group = [group retain];
    [group _addedBookmark:self];
}

+ (WebBookmark *)bookmarkOfType:(WebBookmarkType)type
{
    if (type == WebBookmarkTypeList) {
        return [[[WebBookmarkList alloc] init] autorelease];
    } else if (type == WebBookmarkTypeLeaf) {
        return [[[WebBookmarkLeaf alloc] init] autorelease];
    } else if (type == WebBookmarkTypeSeparator) {
        return [[[WebBookmarkSeparator alloc] init] autorelease];
    }

    return nil;
}


+ (WebBookmark *)bookmarkFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    NSString *typeString;
    
    Class class = nil;
    
    typeString = [dict objectForKey:WebBookmarkTypeKey];
    if ([typeString isEqualToString:WebBookmarkTypeListValue]) {
        class = [WebBookmarkList class];
    } else if ([typeString isEqualToString:WebBookmarkTypeLeafValue]) {
        class = [WebBookmarkLeaf class];
    } else if ([typeString isEqualToString:WebBookmarkTypeSeparatorValue]) {
        class = [WebBookmarkSeparator class];
    }
    
    if (class) {
        return [[[class alloc] initFromDictionaryRepresentation:dict
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

@end
