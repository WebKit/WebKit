//
//  WebBookmarkSeparator.m
//  WebKit
//
//  Created by John Sullivan on Mon May 20 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebBookmarkSeparator.h"

#import "WebBookmarkPrivate.h"
#import <WebKit/WebKitDebug.h>


@implementation WebBookmarkSeparator

- (id)initWithGroup:(WebBookmarkGroup *)group
{
    [super init];
    [self _setGroup:group];

    return self;    
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(WebBookmarkGroup *)group
{
    if (![[dict objectForKey:WebBookmarkTypeKey] isEqualToString:WebBookmarkTypeSeparatorValue]) {
        WEBKITDEBUG("Can't initialize Bookmark separator from non-separator type");
        return nil;
    }

    return [self initWithGroup:group];
}

- (NSDictionary *)dictionaryRepresentation
{
    return [NSDictionary dictionaryWithObject:WebBookmarkTypeSeparatorValue forKey:WebBookmarkTypeKey];
}

- (WebBookmarkType)bookmarkType
{
    return WebBookmarkTypeSeparator;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[WebBookmarkSeparator alloc] initWithGroup:[self group]];
}

- (NSString *)title
{
    return nil;
}

- (NSImage *)icon
{
    return nil;
}

@end
