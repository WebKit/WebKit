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
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebFoundation/WebAssertions.h>

#import <WebFoundation/WebNSURLExtras.h>

#define URIDictionaryKey	@"URIDictionary"
#define URLStringKey		@"URLString"

// FIXME.  This class really shouldn't be using a WebHistoryItem to hold
// it's URL, title and icon.  WebHistoryItem has significantly evolved from
// it original implementation and is no longer appropriate.
@implementation WebBookmarkLeaf

- (id)init
{
    return [self initWithURLString:nil title:nil group:nil];
}

- (id)initWithURLString:(NSString *)URLString
                  title:(NSString *)title
                  group:(WebBookmarkGroup *)group;
{
    [super init];

    // Since our URLString may not be valid for creating an NSURL object,
    // just hang onto the string separately and don't bother creating
    // an NSURL object for the WebHistoryItem.
    _entry = [[WebHistoryItem alloc] init];
    [_entry setTitle:title];	// to avoid sending notifications, don't call setTitle or setURL
    _URLString = [URLString copy];
    [_entry setURL:[NSURL _web_URLWithString:_URLString]];
    [group _addBookmark:self];

    return self;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    ASSERT_ARG(dict, dict != nil);

    self = [super initFromDictionaryRepresentation:dict withGroup:group];

    if (![[dict objectForKey:URIDictionaryKey] isKindOfClass:[NSDictionary class]]
        || ![[dict objectForKey:URLStringKey] isKindOfClass:[NSString class]]) {
        ERROR("bad dictionary");
        [self release];
        return nil;
    }

    // _entry was already created in initWithURLString:title:group, called from super.
    // Releasing it here and creating a new one is inelegant, and we should probably
    // improve this someday (but timing tests show no noticeable slowdown).
    [_entry release];
    _entry = [[WebHistoryItem alloc] initFromDictionaryRepresentation:
        [dict objectForKey:URIDictionaryKey]];
    _URLString = [[dict objectForKey:URLStringKey] copy];

    return self;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = (NSMutableDictionary *)[super dictionaryRepresentation];

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
    WebBookmarkLeaf *copy = [super copyWithZone:zone];

    copy->_entry = [[WebHistoryItem alloc] init];
    [copy->_entry setTitle:[self title]];
    copy->_URLString = [[self URLString] copy];
    [copy->_entry setURL:[NSURL _web_URLWithString:_URLString]];
    
    return copy;
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

    ASSERT(_entry != nil);

    [[self group] _bookmarkWillChange:self];    

    [_entry setTitle:title];

    [[self group] _bookmarkDidChange:self];    
}

- (NSImage *)icon
{
    return [_entry icon];
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
    if (URLString == nil) {
        URLString = @"";
    }
    
    if ([URLString isEqualToString:_URLString]) {
        return;
    }

    [[self group] _bookmarkWillChange:self];    

    [_URLString release];
    _URLString = [URLString copy];

    [_entry setURL:[NSURL _web_URLWithString:_URLString]];

    [[self group] _bookmarkDidChange:self];    
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@: %@", [super description], _URLString];
}

@end
