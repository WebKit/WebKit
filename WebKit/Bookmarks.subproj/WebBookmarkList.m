//
//  WebBookmarkList.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmarkList.h>
#import <WebKit/WebBookmarkLeaf.h>
#import <WebKit/WebBookmarkPrivate.h>
#import <WebKit/WebBookmarkGroupPrivate.h>
#import <WebFoundation/WebAssertions.h>

#define TitleKey		@"Title"
#define ChildrenKey		@"Children"

@implementation WebBookmarkList

- (id)init
{
    return [self initWithTitle:nil group:nil];
}

- (id)initWithTitle:(NSString *)title
              group:(WebBookmarkGroup *)group
{
    [super init];

    _list = [[NSMutableArray alloc] init];
    _title = [title copy];
    [group _addBookmark:self];
    
    return self;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    ASSERT_ARG(dict, dict != nil);

    self = [super initFromDictionaryRepresentation:dict withGroup:group];

    if (![[dict objectForKey:WebBookmarkTypeKey] isKindOfClass:[NSString class]]
        || ([dict objectForKey:TitleKey] && ![[dict objectForKey:TitleKey] isKindOfClass:[NSString class]])
        || ([dict objectForKey:ChildrenKey] && ![[dict objectForKey:ChildrenKey] isKindOfClass:[NSArray class]])) {
        ERROR("bad dictionary");
        [self release];
        return nil;
    }

    if (![[dict objectForKey:WebBookmarkTypeKey] isEqualToString:WebBookmarkTypeListValue]) {
        ERROR("Can't initialize Bookmark list from non-list type");
        [self release];
        return nil;
    }

    _list = [[NSMutableArray alloc] init];
    _title = [[dict objectForKey:TitleKey] copy];

    NSArray *storedChildren = [dict objectForKey:ChildrenKey];
    unsigned count = [storedChildren count];
    unsigned indexRead;
    unsigned indexWritten = 0;
    for (indexRead = 0; indexRead < count; ++indexRead) {
        WebBookmark *child = [WebBookmark bookmarkFromDictionaryRepresentation:[storedChildren objectAtIndex:indexRead]
                                                                     withGroup:group];	
        if (child != nil) {
            [self insertChild:child atIndex:indexWritten++];
        }
    }

    return self;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = (NSMutableDictionary *)[super dictionaryRepresentation];

    if (_title != nil) {
        [dict setObject:_title forKey:TitleKey];
    }

    [dict setObject:WebBookmarkTypeListValue forKey:WebBookmarkTypeKey];

    unsigned childCount = [self numberOfChildren];
    if (childCount > 0) {
        NSMutableArray *childrenAsDictionaries = [NSMutableArray arrayWithCapacity:childCount];

        unsigned index;
        for (index = 0; index < childCount; ++index) {
            WebBookmark *child;

            child = [_list objectAtIndex:index];
            [childrenAsDictionaries addObject:[child dictionaryRepresentation]];
        }

        [dict setObject:childrenAsDictionaries forKey:ChildrenKey];
    }

    return dict;
}

- (void)dealloc
{
    [_title release];
    [_list release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    WebBookmarkList *copy = [super copyWithZone:zone];
    copy->_title = [_title copy];
    copy->_list = [[NSMutableArray alloc] init];
    
    unsigned index;
    unsigned count = [self numberOfChildren];
    for (index = 0; index < count; ++index) {
        WebBookmark *childCopy = [[_list objectAtIndex:index] copyWithZone:zone];
        [copy insertChild:childCopy atIndex:index];
        [childCopy release];
    }

    return copy;
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

    [[self group] _bookmarkWillChange:self];    

    [_title release];
    _title = [title copy];

    [[self group] _bookmarkDidChange:self]; 
}

- (NSImage *)icon
{
    return nil;
}

- (WebBookmarkType)bookmarkType
{
    return WebBookmarkTypeList;
}

- (NSArray *)children
{
    return [NSArray arrayWithArray:_list];
}

- (NSArray *)rawChildren
{
    return _list;
}

- (unsigned)numberOfChildren
{
    return [_list count];
}

- (unsigned)_numberOfDescendants
{
    unsigned result;
    unsigned index, count;
    WebBookmark *child;

    count = [self numberOfChildren];
    result = count;

    for (index = 0; index < count; ++index) {
        child = [_list objectAtIndex:index];
        result += [child _numberOfDescendants];
    }

    return result;
}

- (void)removeChild:(WebBookmark *)bookmark
{
    ASSERT_ARG(bookmark, [bookmark parent] == self);
    ASSERT_ARG(bookmark, [_list indexOfObjectIdenticalTo:bookmark] != NSNotFound);
    
    [bookmark retain];
    [_list removeObjectIdenticalTo:bookmark];
    [bookmark _setParent:nil];
    [[bookmark group] _removeBookmark:bookmark];
    [bookmark release];

    [[self group] _bookmarkChildren:[NSArray arrayWithObject:bookmark] wereRemovedFromParent:self]; 
}


- (void)insertChild:(WebBookmark *)bookmark atIndex:(unsigned)index
{
    ASSERT_ARG(bookmark, [bookmark parent] == nil);
    ASSERT_ARG(bookmark, [_list indexOfObjectIdenticalTo:bookmark] == NSNotFound);

    [_list insertObject:bookmark atIndex:index];
    [bookmark _setParent:self];
    [[self group] _addBookmark:self];

    [[self group] _bookmarkChildren:[NSArray arrayWithObject:bookmark] wereAddedToParent:self]; 
}

@end
