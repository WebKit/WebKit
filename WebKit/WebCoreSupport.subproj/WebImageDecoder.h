/*	
    WebImageDecoder.h
	Copyright 2004, Apple, Inc. All rights reserved.
*/

#ifndef OMIT_TIGER_FEATURES

#import <WebKit/WebImageDecodeItem.h>
@class WebImageData;

@interface WebImageDecoder : NSObject
{
@public
    NSConditionLock *itemLock;
    CFMutableDictionaryRef items;
    NSLock *completedItemsLock;
    NSMutableArray *completedItems;
}
+ (WebImageDecoder *)sharedDecoder;
+ (void)performDecodeWithImage:(WebImageData *)img data:(CFDataRef)d isComplete:(BOOL)f callback:(id)c;
+ (void)decodeComplete:(id)callback status:(CGImageSourceStatus)imageStatus;
+ (BOOL)isImageDecodePending:(WebImageData *)img;
- (WebImageDecodeItem *)removeItem;
- (void)addItem:(WebImageDecodeItem *)item;
- (void)decodeItem:(WebImageDecodeItem *)item;
@end

#endif
