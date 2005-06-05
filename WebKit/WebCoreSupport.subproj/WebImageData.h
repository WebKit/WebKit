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

#ifndef OMIT_TIGER_FEATURES

#import <Cocoa/Cocoa.h>

@class WebImageRenderer;

@interface WebImageData : NSObject <NSCopying>
{
    size_t imagesSize;
    CGImageRef *images;
    size_t imagePropertiesSize;
    CFDictionaryRef fileProperties;
    CFDictionaryRef *imageProperties;
    CGImageSourceRef imageSource;

    CGSize size;
    BOOL haveSize;
    
    CFMutableDictionaryRef animatingRenderers;
    NSTimer *frameTimer;

    size_t frameDurationsSize;
    float *frameDurations;
    
    size_t currentFrame;
    int repetitionsComplete;
    BOOL animationFinished;
    
    NSLock *decodeLock;
    
    id _PDFDoc;
    BOOL isPDF;
    
    BOOL isSolidColor;                              // Is frame 0 a solid color?
    CGColorRef solidColor;                          // If isSolidColor this is the color, or NULL for transparent
    
    BOOL sizeAvailable;
}

- (size_t)numberOfImages;
- (CGImageRef)imageAtIndex:(size_t)index;
- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c;
- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr compositeOperation:(NSCompositingOperation)op context:(CGContextRef)aContext;
- (void)drawImageAtIndex:(size_t)index inRect:(CGRect)ir fromRect:(CGRect)fr adjustedSize:(CGSize)size compositeOperation:(NSCompositingOperation)op context:(CGContextRef)aContext;
- (void)tileInRect:(CGRect)rect fromPoint:(CGPoint)point context:(CGContextRef)aContext;
- (BOOL)isNull;
- (CGSize)size;
- (void)animate;
- (BOOL)shouldAnimate;
+ (void)stopAnimationsInView:(NSView *)aView;
- (void)addAnimatingRenderer:(WebImageRenderer *)r inView:(NSView *)view;
- (void)removeAnimatingRenderer:(WebImageRenderer *)self;
- (BOOL)isAnimationFinished;
- (size_t)currentFrame;
- (CFDictionaryRef)propertiesAtIndex:(size_t)index;

- (void)decodeData:(CFDataRef)data isComplete:(BOOL)f callback:(id)c;

- (void)setIsPDF:(BOOL)f;
- (BOOL)isPDF;

- (void)resetAnimation;

@end

#endif
