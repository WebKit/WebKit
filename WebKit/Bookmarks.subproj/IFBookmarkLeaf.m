//
//  IFBookmarkLeaf.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmarkLeaf.h>
#import <WebKit/IFBookmark_Private.h>
#import <WebKit/IFBookmarkGroup.h>
#import <WebKit/IFBookmarkGroup_Private.h>
#import <WebKit/IFURIEntry.h>
#import <WebKit/WebKitDebug.h>

#define URIDictionaryKey	@"URIDictionary"
#define URLStringKey		@"URLString"

@implementation IFBookmarkLeaf

- (id)initWithURLString:(NSString *)URLString
                  title:(NSString *)title
                  image:(NSImage *)image
                  group:(IFBookmarkGroup *)group;
{
    WEBKIT_ASSERT_VALID_ARG (group, group != nil);
    
    [super init];

    // Since our URLString may not be valid for creating an NSURL object,
    // just hang onto the string separately and don't bother creating
    // an NSURL object for the IFURIEntry.
    _entry = [[IFURIEntry alloc] initWithURL:nil title:title image:image];
    _URLString = [URLString retain];
    [self _setGroup:group];

    return self;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group
{
    WEBKIT_ASSERT_VALID_ARG (dict, dict != nil);

    [super init];

    [self _setGroup:group];
    
    _entry = [[[IFURIEntry alloc] initFromDictionaryRepresentation:
        [dict objectForKey:URIDictionaryKey]] retain];
    _URLString = [[dict objectForKey:URLStringKey] retain];

    return self;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict;

    dict = [NSMutableDictionary dictionaryWithCapacity: 3];

    [dict setObject:IFBookmarkTypeLeafValue forKey:IFBookmarkTypeKey];
    [dict setObject:[_entry dictionaryRepresentation] forKey:URIDictionaryKey];
    if (_URLString != nil) {
        [dict setObject:_URLString forKey:URLStringKey];
    }

    return dict;
}

- (void)dealloc
{
    [_entry release];
    [_URLString release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[IFBookmarkLeaf alloc] initWithURLString:_URLString
                                               title:[self title]
                                               image:[self image]
                                               group:[self group]];
}

- (NSString *)title
{
    return [_entry title];
}

- (void)setTitle:(NSString *)title
{
    if ([title isEqualToString:[self title]]) {
        return;
    }
    
    [_entry setTitle:title];

    [[self group] _bookmarkDidChange:self];    
}

- (NSImage *)image
{
    return [_entry image];
}

- (void)setImage:(NSImage *)image
{
    [_entry setImage:image];

    [[self group] _bookmarkDidChange:self];    
}

- (IFBookmarkType)bookmarkType
{
    return IFBookmarkTypeLeaf;
}

- (NSString *)URLString
{
    return _URLString;
}

- (void)setURLString:(NSString *)URLString
{
    if ([URLString isEqualToString:_URLString]) {
        return;
    }

    [_URLString release];
    _URLString = [URLString copy];

    [[self group] _bookmarkDidChange:self];    
}


@end
