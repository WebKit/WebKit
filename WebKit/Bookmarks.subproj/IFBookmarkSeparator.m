//
//  IFBookmarkSeparator.m
//  WebKit
//
//  Created by John Sullivan on Mon May 20 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFBookmarkSeparator.h"

#import "IFBookmark_Private.h"
#import <WebKit/WebKitDebug.h>


@implementation IFBookmarkSeparator

- (id)initWithGroup:(IFBookmarkGroup *)group
{
    WEBKIT_ASSERT_VALID_ARG (group, group != nil);

    [super init];
    [self _setGroup:group];

    return self;    
}

- (id)_initFromDictionaryRepresentation:(NSDictionary *)dict withGroup:(IFBookmarkGroup *)group
{
    if (![[dict objectForKey:IFBookmarkTypeKey] isEqualToString:IFBookmarkTypeSeparatorValue]) {
        WEBKITDEBUG("Can't initialize Bookmark separator from non-separator type");
        return nil;
    }

    return [self initWithGroup:group];
}

- (NSDictionary *)_dictionaryRepresentation
{
    return [NSDictionary dictionaryWithObject:IFBookmarkTypeSeparatorValue forKey:IFBookmarkTypeKey];
}

- (IFBookmarkType)bookmarkType
{
    return IFBookmarkTypeSeparator;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[IFBookmarkSeparator alloc] initWithGroup:[self group]];
}

@end
