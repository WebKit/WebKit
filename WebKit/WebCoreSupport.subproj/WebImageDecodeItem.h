/*	
    WebImageDecodeItem.h
	Copyright 2004, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebImageData.h>

#ifndef OMIT_TIGER_FEATURES

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