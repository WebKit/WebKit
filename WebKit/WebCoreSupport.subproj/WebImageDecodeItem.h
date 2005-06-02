/*	
    WebImageDecodeItem.h
	Copyright 2004, Apple, Inc. All rights reserved.
*/
#ifndef OMIT_TIGER_FEATURES

#import <Cocoa/Cocoa.h>

@class WebImageData;

@interface WebImageDecodeItem : NSObject
{
@public
    WebImageData *imageData;
    id callback;
    CFDataRef data;
    BOOL isComplete;
}
+ decodeItemWithImage:(WebImageData *)img data:(CFDataRef)d isComplete:(BOOL)f callback:(id)c;
- initWithImage:(WebImageData *)img data:(CFDataRef)d isComplete:(BOOL)f callback:(id)c;
@end

#endif
