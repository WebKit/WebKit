//
//  WebBookmarkLeaf.m
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmarkLeaf.h>
#import <WebKit/WebBookmarkPrivate.h>
#import <WebKit/WebBookmarkGroup.h>
#import <WebKit/WebBookmarkGroupPrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebKitDebug.h>

#define URIDictionaryKey	@"URIDictionary"
#define URLStringKey		@"URLString"

@implementation WebBookmarkLeaf

- (id)init
{
    [super init];
    _entry = [[WebHistoryItem alloc] init];
    return self;
}

- (id)initWithURLString:(NSString *)URLString
                  title:(NSString *)title
                  image:(NSImage *)image
                  group:(WebBookmarkGroup *)group;
{
    WEBKIT_ASSERT_VALID_ARG (group, group != nil);
    
    [self init];

    // Since our URLString may not be valid for creating an NSURL object,
    // just hang onto the string separately and don't bother creating
    // an NSURL object for the WebHistoryItem.
    [self setTitle:title];
    [self setImage:image];
    _URLString = [URLString retain];
    [self _setGroup:group];

    return self;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    WEBKIT_ASSERT_VALID_ARG (dict, dict != nil);

    [super init];

    [self _setGroup:group];
    
    _entry = [[[WebHistoryItem alloc] initFromDictionaryRepresentation:
        [dict objectForKey:URIDictionaryKey]] retain];
    _URLString = [[dict objectForKey:URLStringKey] retain];

    return self;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict;

    dict = [NSMutableDictionary dictionaryWithCapacity:3];

    [dict setObject:WebBookmarkTypeLeafValue forKey:WebBookmarkTypeKey];
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
    return [[WebBookmarkLeaf allocWithZone:zone] initWithURLString:_URLString
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

    WEBKIT_ASSERT (_entry != nil);
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

- (WebBookmarkType)bookmarkType
{
    return WebBookmarkTypeLeaf;
}

- (NSString *)URLString
{
    return _URLString;
}

- (void)setURLString:(NSString *)URLString
{
    WEBKIT_ASSERT_VALID_ARG (URLString, URLString != nil);
    if ([URLString isEqualToString:_URLString]) {
        return;
    }

    [_URLString release];
    _URLString = [URLString copy];

    [[self group] _bookmarkDidChange:self];    
}

@end
