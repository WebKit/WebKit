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

#import <Cocoa/Cocoa.h>

@protocol WebCoreImageRenderer;

@class WebImageData;

@interface WebImageRenderer : NSObject <WebCoreImageRenderer>
{
    NSString *MIMEType;

    WebImageData *imageData;

    NSImage *nsimage;
    NSData *TIFFData;
    
    NSRect targetAnimationRect;
    
    NSSize adjustedSize;
    BOOL isSizeAdjusted;
}

- (id)initWithMIMEType:(NSString *)MIME;
- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME;
- (id)initWithContentsOfFile:(NSString *)filename;
- (NSString *)MIMEType;
+ (void)stopAnimationsInView:(NSView *)aView;
- (int)frameCount;
- (NSRect)targetAnimationRect;
- (void)resize:(NSSize)s;
- (NSSize)size;
- (NSData *)TIFFRepresentation;
- (NSImage *)image;
- (CGImageRef)imageRef;

@end

// FIXME: This class needs its own header and source file.
@interface WebPDFDocument : NSObject
{
    CGPDFDocumentRef _document;
    CGRect           _mediaBox;
    NSRect           _cropBox;
    float            _rotation;
    int              _currentPage;
}
- (id)               initWithData:(NSData*)data;
- (CGPDFDocumentRef) documentRef;
- (CGRect)           mediaBox;
- (NSRect)           bounds;	// adjust for rotation
- (void)             setCurrentPage:(int)page;
- (int)              currentPage;
- (int)              pageCount;
- (void)             adjustCTM:(CGContextRef)context;
@end
