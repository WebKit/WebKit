/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#if !PLATFORM(IOS_FAMILY)

#import <WebKitLegacy/WebNSImageExtras.h>

#import <WebKitLegacy/WebKitLogging.h>

@implementation NSImage (WebExtras)

- (void)_web_scaleToMaxSize:(NSSize)size
{
    float heightResizeDelta = 0.0f, widthResizeDelta = 0.0f, resizeDelta = 0.0f;
    NSSize originalSize = [self size];

    if(originalSize.width > size.width){
        widthResizeDelta = size.width / originalSize.width;
        resizeDelta = widthResizeDelta;
    }

    if(originalSize.height > size.height){
        heightResizeDelta = size.height / originalSize.height;
        if((resizeDelta == 0.0) || (resizeDelta > heightResizeDelta)){
            resizeDelta = heightResizeDelta;
        }
    }
    
    if(resizeDelta > 0.0){
        NSSize newSize = NSMakeSize((originalSize.width * resizeDelta), (originalSize.height * resizeDelta));
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [self setScalesWhenResized:YES];
        ALLOW_DEPRECATED_DECLARATIONS_END
        [self setSize:newSize];
    }
}

- (void)_web_dissolveToFraction:(float)delta
{
    NSImage *dissolvedImage = [[NSImage alloc] initWithSize:[self size]];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPoint point = [self isFlipped] ? NSMakePoint(0, [self size].height) : NSZeroPoint;
    ALLOW_DEPRECATED_DECLARATIONS_END
    
    // In this case the dragging image is always correct.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [dissolvedImage setFlipped:[self isFlipped]];
    ALLOW_DEPRECATED_DECLARATIONS_END

    [dissolvedImage lockFocus];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [self dissolveToPoint:point fraction: delta];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [dissolvedImage unlockFocus];

    [self lockFocus];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [dissolvedImage compositeToPoint:point operation:NSCompositeCopy];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [self unlockFocus];

    [dissolvedImage release];
}

@end

#endif // !PLATFORM(IOS_FAMILY)
