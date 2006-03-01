/*
 * Copyright (C) 2003, 2005 Apple Computer, Inc.  All rights reserved.
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

#import <Cocoa/Cocoa.h>

typedef enum {
    WebImageStretch,
    WebImageRound,
    WebImageRepeat
} WebImageTileRule;

@protocol WebCoreImageRenderer <NSObject, NSCopying>

- (BOOL)incrementalLoadWithBytes:(const void *)bytes length:(unsigned)length complete:(BOOL)isComplete callback:(id)c;

- (NSSize)size;
- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr;
- (void)drawImageInRect:(NSRect)ir fromRect:(NSRect)fr compositeOperator:(NSCompositingOperation)compsiteOperator context:(CGContextRef)context;
- (void)stopAnimation;
- (void)tileInRect:(NSRect)r fromPoint:(NSPoint)p context:(CGContextRef)context;
- (void)scaleAndTileInRect:(NSRect)ir fromRect:(NSRect)fr withHorizontalTileRule:(WebImageTileRule)hRule withVerticalTileRule:(WebImageTileRule)vRule context:(CGContextRef)context;
- (BOOL)isNull;
- (id <WebCoreImageRenderer>)retainOrCopyIfNeeded;
- (void)resetAnimation;
- (void)setAnimationRect:(NSRect)r;

- (CGImageRef)imageRef;
- (NSImage *)image;
- (NSData *)TIFFRepresentation;

@end
