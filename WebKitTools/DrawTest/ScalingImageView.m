/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */


#import "ScalingImageView.h"

@implementation ScalingImageView

/*
 This class offers two behaviors different from the standard NSImageView
 (and which were not available w/o subclassing)
 1.  Scale an image proportionally up to fit a larger view (NSImageView refuses)
 2.  Draw a background color w/o needing to show a bezel.
*/

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    
    NSSize imageSize = [[self image] size];
    float scale = 1.0f;
    if ([self imageScaling] == NSScaleProportionally && imageSize.width && imageSize.height) {
        float widthScale = [self bounds].size.width / imageSize.width;
        float heightScale = [self bounds].size.height / imageSize.height;
        scale = MIN(widthScale, heightScale);
    }

    float scaledHeight = imageSize.height * scale;
    NSRect destRect = NSMakeRect(0,[self bounds].size.height - scaledHeight,imageSize.width * scale, scaledHeight);
    [[self image] drawInRect:destRect
        fromRect:NSMakeRect(0,0,imageSize.width, imageSize.height) operation:NSCompositeSourceOver fraction:1.0];
}

@end
