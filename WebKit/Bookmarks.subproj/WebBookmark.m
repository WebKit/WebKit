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
    // a bookmark must be removed from its group before this,
    // so the UUID table removes its (optional) entry for the bookmark.
    ASSERT (_group == nil);
    
    [_identifier release];
    [_UUID release];
    
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    WebBookmark *copy = NSCopyObject(self, 0, zone);
    copy->_group = nil;
    copy->_parent = nil;
    copy->_identifier = nil;
    copy->_UUID = nil;

    [copy setIdentifier:[self identifier]];

    // UUID starts the same as the original, which is OK
    // since the copy isn't in a group yet. When it's added
    // to a group, the UUID will be uniqued if necessary at that time.
    if ([self _hasUUID]) {
        [copy _setUUID:[self UUID]];
    }
    
    // parent and group are left nil for fresh copies

    return copy;
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

- (NSArray *)rawChildren
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

- (void)_setUUID:(NSString *)UUID
{
    ASSERT(_UUID == nil || UUID == nil);

    NSString *oldUUID = _UUID;
    _UUID = [UUID copy];

    [[self group] _bookmark:self changedUUIDFrom:oldUUID to:_UUID];
    [oldUUID release];
}

- (NSString *)UUID
{
    // lazily generate
    if (_UUID == nil) {
        CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
        _UUID = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
        [[self group] _bookmark:self changedUUIDFrom:nil to:_UUID];
        CFRelease(UUIDRef);
    }
    
    return _UUID;
}

- (BOOL)_hasUUID
{
    return _UUID != nil;
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
    [self init];

    [self setIdentifier:[dict objectForKey:WebBookmarkIdentifierKey]];
    [self _setUUID:[dict objectForKey:WebBookmarkUUIDKey]];
    [group _addBookmark:self];

    return self;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if ([self identifier] != nil) {
        [dict setObject:[self identifier] forKey:WebBookmarkIdentifierKey];
    }
    // UUID is generated lazily; guaranteed to be non-nil
    [dict setObject:[self UUID] forKey:WebBookmarkUUIDKey];

    return dict;
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
        NSArray *theseChildren = [self rawChildren];
        NSArray *thoseChildren = [otherBookmark rawChildren];
        if (![[theseChildren objectAtIndex:childIndex] contentMatches:[thoseChildren objectAtIndex:childIndex]]) {
            return NO;
        }
    }

    return YES;
}


@end
