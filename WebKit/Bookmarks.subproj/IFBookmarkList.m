//
//  IFBookmarkList.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkList.h>
#import <WebKit/IFBookmarkLeaf.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/IFBookmarkGroup_Private.h>
#import <WebKit/WebKitDebug.h>

#define TitleKey		@"Title"
#define ListIdentifierKey	@"ListIdentifier"
#define ChildrenKey		@"Children"

@implementation IFBookmarkList

- (id)initWithTitle:(NSString *)title
              image:(NSImage *)image
              group:(IFBookmarkGroup *)group
{
    WEBKIT_ASSERT_VALID_ARG (group, group != nil);
    
    [super init];

    _title = [title copy];
    _image = [image retain];
    _list = [[NSMutableArray alloc] init];
    [self _setGroup:group];
    
    return self;
}

- (id)_initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group
{
    NSArray *storedChildren;
    NSDictionary *childAsDictionary;
    IFBookmark *child;
    unsigned index, count;
    
    WEBKIT_ASSERT_VALID_ARG (dict, dict != nil);

    [super init];

    [self _setGroup:group];

    // FIXME: doesn't restore images
    _title = [[dict objectForKey:TitleKey] retain];
    _list = [[NSMutableArray alloc] init];

    storedChildren = [dict objectForKey:ChildrenKey];
    if (storedChildren != nil) {
        count = [storedChildren count];
        for (index = 0; index < count; ++index) {
            childAsDictionary = [storedChildren objectAtIndex:index];
            
            // determine whether child is a leaf or a list by looking for the
            // token that this list class inserts.
            if ([childAsDictionary objectForKey:ListIdentifierKey] != nil) {
                child = [[IFBookmarkList alloc] _initFromDictionaryRepresentation:childAsDictionary
                                                                        withGroup:group];
            } else {
                child = [[IFBookmarkLeaf alloc] _initFromDictionaryRepresentation:childAsDictionary
                                                                        withGroup:group];
            }

            [self insertChild:child atIndex:index];
        }
    }

    return self;
}

- (NSDictionary *)_dictionaryRepresentation
{
    NSMutableDictionary *dict;
    NSMutableArray *childrenAsDictionaries;
    unsigned index, childCount;

    dict = [NSMutableDictionary dictionaryWithCapacity: 3];

    // FIXME: doesn't save images
    if (_title != nil) {
        [dict setObject:_title forKey:TitleKey];
    }

    // mark this as a list-type bookmark; used in _initFromDictionaryRepresentation
    [dict setObject:@"YES" forKey:ListIdentifierKey];

    childCount = [self numberOfChildren];
    if (childCount > 0) {
        childrenAsDictionaries = [NSMutableArray arrayWithCapacity:childCount];

        for (index = 0; index < childCount; ++index) {
            IFBookmark *child;

            child = [_list objectAtIndex:index];
            [childrenAsDictionaries addObject:[child _dictionaryRepresentation]];
        }

        [dict setObject:childrenAsDictionaries forKey:ChildrenKey];
    }
    
    return dict;
}

- (void)dealloc
{
    [_title release];
    [_image release];
    [_list release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    IFBookmarkList *copy;
    unsigned index, count;
    
    copy = [[IFBookmarkList alloc] initWithTitle:[self title]
                                           image:[self image]
                                           group:[self _group]];

    count = [self numberOfChildren];
    for (index = 0; index < count; ++index) {
        IFBookmark *childCopy;

        childCopy = [[[_list objectAtIndex:index] copyWithZone:zone] autorelease];
        [copy insertChild:childCopy atIndex:index];
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

    [_title release];
    _title = [title copy];

    [[self _group] _bookmarkDidChange:self]; 
}

- (NSImage *)image
{
    static NSImage *defaultImage = nil;
    static BOOL loadedDefaultImage = NO;

    if (_image != nil) {
        return _image;
    }
    
    // Attempt to load default image only once, to avoid performance penalty of repeatedly
    // trying and failing to find it.
    if (!loadedDefaultImage) {
        NSString *pathForDefaultImage =
        [[NSBundle bundleForClass:[self class]] pathForResource:@"bookmark_folder" ofType:@"tiff"];
        if (pathForDefaultImage != nil) {
            defaultImage = [[NSImage alloc] initByReferencingFile: pathForDefaultImage];
        }
        loadedDefaultImage = YES;
    }

    return defaultImage;
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

- (unsigned)_numberOfDescendants
{
    unsigned result;
    unsigned index, count;
    IFBookmark *child;

    count = [self numberOfChildren];
    result = count;

    for (index = 0; index < count; ++index) {
        child = [_list objectAtIndex:index];
        result += [child _numberOfDescendants];
    }

    return result;
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

- (void)_setGroup:(IFBookmarkGroup *)group
{
    if (group == [self _group]) {
        return;
    }

    [super _setGroup:group];
    [_list makeObjectsPerformSelector:@selector(_setGroup:) withObject:group];
}

@end
