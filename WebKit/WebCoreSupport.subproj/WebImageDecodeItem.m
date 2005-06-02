/*	
    WebImageDecodeItem.m
	Copyright 2004, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebImageDecodeItem.h>
#import <WebKit/WebImageData.h>

#ifndef OMIT_TIGER_FEATURES

@implementation WebImageDecodeItem

+ decodeItemWithImage:(WebImageData *)img data:(CFDataRef)d isComplete:(BOOL)f callback:(id)c
{
    return [[[WebImageDecodeItem alloc] initWithImage:img data:d isComplete:f callback:c] autorelease];
}

- initWithImage:(WebImageData *)img data:(CFDataRef)d isComplete:(BOOL)f callback:(id)c
{
    self = [super init];
    imageData = [img retain];
    callback = [c retain];
    data = CFRetain (d);
    isComplete = f;
    return self;
}

- (void)finalize
{
    CFRelease (data);
    [super finalize];
}

- (void)dealloc
{
    [imageData release];
    [callback release];
    CFRelease (data);
    [super dealloc];
}

@end

#endif
