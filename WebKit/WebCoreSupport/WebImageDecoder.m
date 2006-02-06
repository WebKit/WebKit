/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebImageDecoder.h"

#import "WebAssertions.h"
#import "WebImageData.h"

@interface WebImageCallback : NSObject
- (void)notify;
- (void)setImageSourceStatus:(CGImageSourceStatus)status;
- (CGImageSourceStatus)status;
@end


@implementation WebImageDecoder

#define CONDITION_NEW_ITEM_TO_DECODE 1
#define CONDITION_CHECKED_FOR_ITEMS 2

static void startDecoderThread(void);
static void *decoderThread(void *arg);

static CFRunLoopSourceRef decoderMainThreadSource;
static CFRunLoopRef mainRunLoop;

// Don't send partial image update notifications more than
// every PARTIAL_IMAGE_UPDATE_INTERVAL seconds.  The is
// a global update interval for all pages in all windows.
// The update notification will trigger a paint, so governing
// the notification limits the number of intermediate paints
// for progressive images.  Not sending a partial
// update is harmless.
//
// Note the completed notifications are always sent immediately,
// so, the PARTIAL_IMAGE_UPDATE_INTERVAL will not ever delay
// drawing of the completely decoded image.
//
// We may want to make this interval dynamic based on speed of the 
// machine.  
#define PARTIAL_IMAGE_UPDATE_INTERVAL 0.2

static double lastPartialImageUpdate;

static void decoderNotifications(void *info);
static void decoderNotifications(void *info)
{
    WebImageDecoder *decoder = [WebImageDecoder sharedDecoder];
 
    ASSERT (mainRunLoop == CFRunLoopGetCurrent());
   
    [decoder->completedItemsLock lock];
    
    unsigned i, count = [decoder->completedItems count];
    WebImageCallback *callback;
    for (i = 0; i < count; i++) {
        callback = [decoder->completedItems objectAtIndex:i];
        if ([callback status] <= kCGImageStatusIncomplete) {
            double now = CFAbsoluteTimeGetCurrent();
            if (lastPartialImageUpdate != 0 && lastPartialImageUpdate + PARTIAL_IMAGE_UPDATE_INTERVAL > now)
                continue;
            lastPartialImageUpdate = now;
        }
        [callback notify];
    }
    [decoder->completedItems removeAllObjects];
    
    [decoder->completedItemsLock unlock];
}

+ (void)initialize
{
    // Assumes we are being called from main thread.

    // Create shared decoder.
    [WebImageDecoder sharedDecoder];
    
    // Add run loop source to receive notifications when decoding has completed.
    mainRunLoop = CFRunLoopGetCurrent();
    CFRunLoopSourceContext sourceContext = {0, self, NULL, NULL, NULL, NULL, NULL, NULL, NULL, decoderNotifications};
    decoderMainThreadSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(mainRunLoop, decoderMainThreadSource, kCFRunLoopDefaultMode);
    
    startDecoderThread();
}

+ (void)notifyMainThread
{
    CFRunLoopSourceSignal(decoderMainThreadSource);
    if (CFRunLoopIsWaiting(mainRunLoop))
        CFRunLoopWakeUp(mainRunLoop);
}

+ (WebImageDecoder *)sharedDecoder
{
    static WebImageDecoder *sharedDecoder = 0;
    
    if (!sharedDecoder)
        sharedDecoder = [[WebImageDecoder alloc] init];
    
    return sharedDecoder;
}

+ (void)performDecodeWithImage:(WebImageData *)img data:(CFDataRef)d isComplete:(BOOL)f callback:(id)c
{
    WebImageDecodeItem *item = [WebImageDecodeItem decodeItemWithImage:img data:d isComplete:(BOOL)f callback:c];
    [[WebImageDecoder sharedDecoder] addItem:item];
}

+ (BOOL)isImageDecodePending:(WebImageData *)img;
{
    WebImageDecoder *decoder = [WebImageDecoder sharedDecoder];
    
    [decoder->itemLock lock];
    BOOL result = CFDictionaryGetValue (decoder->items, img) ? YES : NO;
    [decoder->itemLock unlock];
    
    return result;
}

+ (BOOL)imageDecodesPending
{
    WebImageDecoder *decoder = [WebImageDecoder sharedDecoder];

    [decoder->itemLock lock];
    BOOL result = CFDictionaryGetCount(decoder->items) > 0 ? YES : NO;
    [decoder->itemLock unlock];
    
    return result;
}

+ (void)decodeComplete:(id)c status:(CGImageSourceStatus)imageStatus
{
    WebImageDecoder *decoder = [WebImageDecoder sharedDecoder];
    WebImageCallback *callback = (WebImageCallback *)c;
    [c setImageSourceStatus:imageStatus];
    
    [decoder->completedItemsLock lock];
    [decoder->completedItems addObject:callback];
    [decoder->completedItemsLock unlock];

    [WebImageDecoder notifyMainThread];
}


- init
{
    self = [super init];
    items = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    itemLock = [[NSConditionLock alloc] init];
    completedItems = [[NSMutableArray alloc] init];
    completedItemsLock = [[NSLock alloc] init];
    return self;
}

- (void)dealloc
{
    CFRelease (items);
    [itemLock release];
    [completedItems release];
    [completedItemsLock release];
    [super dealloc];
}

- (void)finalize
{
    CFRelease (items);
    [super finalize];
}

#define ITEM_STACK_BUFFER_SIZE 128

- (WebImageDecodeItem *)removeItem
{
    ASSERT (mainRunLoop != CFRunLoopGetCurrent());

    WebImageDecodeItem *item = 0;
    
    [itemLock lock];
    
    CFIndex count = CFDictionaryGetCount(items);
    if (count) {
        WebImageData *_keys[ITEM_STACK_BUFFER_SIZE], **keys;
        WebImageDecodeItem *_values[ITEM_STACK_BUFFER_SIZE], **values;
        if (count > ITEM_STACK_BUFFER_SIZE) {
            keys = (WebImageData **)malloc(count * sizeof(WebImageData *));
            values = (WebImageDecodeItem **)malloc(count * sizeof(WebImageDecodeItem *));
        }
        else {
            keys = _keys;
            values = _values;
        }
        CFDictionaryGetKeysAndValues (items, (const void **)keys, (const void **)values);
        
        item = _values[0];

        CFDictionaryRemoveValue (items, _keys[0]);
        
        if (keys != _keys) {
            free (keys);
            free (values);
        }
    }
    [itemLock unlock];
    
    // If we have no item block until we have a new item.
    if (!item) {
        [itemLock lockWhenCondition:CONDITION_NEW_ITEM_TO_DECODE];
        [itemLock unlockWithCondition:CONDITION_CHECKED_FOR_ITEMS];
    }
    
    return item;
}

- (void)addItem:(WebImageDecodeItem *)item
{
    ASSERT (mainRunLoop == CFRunLoopGetCurrent());

    [itemLock lock];
    
    // Add additional reference here to ensure that the main thread's pool
    // doesn't release the item.  It has to stick around long enough for
    // the decode thread to process it.  The decoder thread will remove
    // the extra reference.
    [item retain];
    
    // If a prior decode item was already requested
    // for the image, remove it.  The new item will have more
    // data.  Necessary because CFDictionaryAddValue has a 
    // "add if absent" policy.
    WebImageDecodeItem *replacementItem = (WebImageDecodeItem *)CFDictionaryGetValue (items, item->imageData);
    if (replacementItem) {
        CFDictionaryRemoveValue (items, item->imageData);
        // Remove additional reference increment when item was added.
        [replacementItem release];
    }

    CFDictionaryAddValue (items, item->imageData, item);
        
    [itemLock unlockWithCondition:CONDITION_NEW_ITEM_TO_DECODE];
}

- (void)decodeItem:(WebImageDecodeItem *)item
{
    ASSERT (mainRunLoop != CFRunLoopGetCurrent());

    [item->imageData decodeData:item->data isComplete:item->isComplete callback:item->callback];

    // Remove additional reference incremented when item was added.
    [item release];
}

@end

static void *decoderThread(void *arg)
{    
    WebImageDecoder *decoder = [WebImageDecoder sharedDecoder];
    WebImageDecodeItem *item;
    
    while (true) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

        item = [decoder removeItem];
        if (item) {
            [decoder decodeItem:item];
        }

        [pool release];
    }
    
    return 0;
}

static void startDecoderThread(void)
{
    pthread_attr_t attr;
    pthread_t tid;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, decoderThread, 0);
    pthread_attr_destroy(&attr);
}
