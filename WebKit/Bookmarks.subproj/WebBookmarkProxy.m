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

- (id)initWithTitle:(NSString *)title group:(WebBookmarkGroup *)group;
{
    [super init];
    [self _setGroup:group];
    [self setTitle:title];

    return self;    
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    if (![[dict objectForKey:WebBookmarkTypeKey] isKindOfClass:[NSString class]]) {
        ERROR("bad dictionary");
        return nil;
    }
    if (![[dict objectForKey:WebBookmarkTypeKey] isEqualToString:WebBookmarkTypeProxyValue]) {
        ERROR("Can't initialize Bookmark proxy from non-proxy type");
        return nil;
    }

    return [self initWithTitle:[dict objectForKey:TitleKey] group:group];
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:2];
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
    return [[WebBookmarkProxy alloc] initWithTitle:_title group:[self group]];
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

    [_title release];
    _title = [newTitle copy];
    [[self group] _bookmarkDidChange:self]; 
}

- (NSImage *)icon
{
    return nil;
}

@end
