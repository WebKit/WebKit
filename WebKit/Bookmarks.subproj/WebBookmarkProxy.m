//
//  WebBookmarkProxy.m
//  WebKit
//
//  Created by John Sullivan on Fri Oct 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebBookmarkProxy.h"

#import "WebBookmarkPrivate.h"
#import "WebBookmarkGroupPrivate.h"
#import <WebFoundation/WebAssertions.h>

#define TitleKey		@"Title"

@implementation WebBookmarkProxy

- (id)init
{
    return [self initWithTitle:nil group:nil];
}

- (id)initWithTitle:(NSString *)title group:(WebBookmarkGroup *)group;
{
    [super init];
    [group _addBookmark:self];
    _title = [title copy];	// to avoid sending notifications, don't call setTitle

    return self;    
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    self = [super initFromDictionaryRepresentation:dict withGroup:group];
    
    if (![[dict objectForKey:WebBookmarkTypeKey] isKindOfClass:[NSString class]]) {
        ERROR("bad dictionary");
        [self release];
        return nil;
    }
    if (![[dict objectForKey:WebBookmarkTypeKey] isEqualToString:WebBookmarkTypeProxyValue]) {
        ERROR("Can't initialize Bookmark proxy from non-proxy type");
        [self release];
        return nil;
    }

    _title = [[dict objectForKey:TitleKey] copy];

    return self;
}

- (void)dealloc
{
    [_title release];
    [super dealloc];
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = (NSMutableDictionary *)[super dictionaryRepresentation];
    [dict setObject:WebBookmarkTypeProxyValue forKey:WebBookmarkTypeKey];
    if (_title != nil) {
        [dict setObject:_title forKey:TitleKey];
    }

    return dict;
}

- (WebBookmarkType)bookmarkType
{
    return WebBookmarkTypeProxy;
}

- (id)copyWithZone:(NSZone *)zone
{
    WebBookmarkProxy *copy = [super copyWithZone:zone];
    copy->_title = [_title copy];
    return copy;
}

- (NSString *)title
{
    return _title;
}

- (void)setTitle:(NSString *)newTitle
{
    if (_title == newTitle) {
        return;
    }

    [[self group] _bookmarkWillChange:self];    

    [_title release];
    _title = [newTitle copy];

    [[self group] _bookmarkDidChange:self]; 
}

- (NSImage *)icon
{
    return nil;
}

@end
