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

/*
 WARNING: These files are temporary and exist solely for the purpose
 of allowing greater testing of WebCore+SVG prior to full DOM integration.
 Do NOT depend on these SPIs or files as they will soon be gone.
*/

/*
 NOTE: This specific file exists as a hack to allow WebKit (and the test 
 apps which link against WebCore+SVG) to pass an SVG file to NSImage 
 [NSImage imageWithContentsOfFile:]
 and have NSImage be able to handle the SVG correctly.
*/

#import <Cocoa/Cocoa.h>

@class DrawDocument;
@class DrawView;

@interface NSSVGImageRep : NSImageRep {
    DrawDocument *_drawDocument;
    DrawView *_view;
    
    NSBitmapImageRep *_cachedRepHack; // FIXME: total (temporary) HACK
}

+ (id)imageRepWithData:(NSData *)svgData;
- (id)initWithData:(NSData *)svgData;
@end
