//
//  IFBookmark.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmark.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/IFBookmarkGroup.h>
#import <WebKit/IFBookmarkGroup_Private.h>
#import <WebKit/IFBookmarkLeaf.h>
#import <WebKit/IFBookmarkList.h>
#import <WebKit/IFBookmarkSeparator.h>
#import <WebKit/WebKitDebug.h>

// to get NSRequestConcreteImplementation
#import <Foundation/NSPrivateDecls.h>

@implementation IFBookmark

static unsigned _highestUsedID = 0;

+ (NSString *)_generateUniqueIdentifier
{
    return [[NSNumber numberWithInt:++_highestUsedID] stringValue];
}

- (id)init
{
    [super init];
    _identifier = [[IFBookmark _generateUniqueIdentifier] retain];
    return self;
}

- (void)dealloc
{
    WEBKIT_ASSERT (_group == nil);

    [_identifier release];    
    [super dealloc];
}

- (NSString *)identifier
{
    WEBKIT_ASSERT(_identifier != nil);
    return [[_identifier retain] autorelease];
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

- (NSImage *)image
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
    return nil;
}

- (void)setImage:(NSImage *)image
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
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

- (void)insertChild:(IFBookmark *)bookmark atIndex:(unsigned)index
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (void)removeChild:(IFBookmark *)bookmark
{
    NSRequestConcreteImplementation(self, _cmd, [self class]);
}

- (IFBookmark *)parent
{
    return _parent;
}

- (void)_setParent:(IFBookmark *)parent
{
    // Don't retain parent, to avoid circular ownership that prevents dealloc'ing
    // when a parent with children is removed from a group and has no other references.
    _parent = parent;
}

- (IFBookmarkGroup *)group
{
    return _group;
}

- (void)_setGroup:(IFBookmarkGroup *)group
{
    if (group == _group) {
        return;
    }

    [_group _removedBookmark:self];
    [_group release];

    _group = [group retain];
    [group _addedBookmark:self];
}

+ (IFBookmark *)bookmarkOfType:(IFBookmarkType)type
{
    if (type == IFBookmarkTypeList) {
        return [[[IFBookmarkList alloc] init] autorelease];
    } else if (type == IFBookmarkTypeLeaf) {
        return [[[IFBookmarkLeaf alloc] init] autorelease];
    } else if (type == IFBookmarkTypeSeparator) {
        return [[[IFBookmarkSeparator alloc] init] autorelease];
    }

    return nil;
}


+ (IFBookmark *)bookmarkFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group
{
    NSString *typeString;
    
    Class class = nil;
    
    typeString = [dict objectForKey:IFBookmarkTypeKey];
    if ([typeString isEqualToString:IFBookmarkTypeListValue]) {
        class = [IFBookmarkList class];
    } else if ([typeString isEqualToString:IFBookmarkTypeLeafValue]) {
        class = [IFBookmarkLeaf class];
    } else if ([typeString isEqualToString:IFBookmarkTypeSeparatorValue]) {
        class = [IFBookmarkSeparator class];
    }
    
    if (class) {
        return [[[class alloc] initFromDictionaryRepresentation:dict
                                                      withGroup:group] autorelease];
    }

    return nil;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group
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
