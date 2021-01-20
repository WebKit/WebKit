/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <CoreGraphics/CoreGraphics.h>
#import <WebKitLegacy/DOMCSS.h>
#import <WebKitLegacy/DOMHTML.h>
#import <WebKitLegacy/DOMRange.h>

@class NSArray;
@class NSImage;
@class NSURL;

#if TARGET_OS_IPHONE

@interface DOMHTMLElement (DOMHTMLElementExtensions)
- (int)scrollXOffset;
- (int)scrollYOffset;
- (void)setScrollXOffset:(int)x scrollYOffset:(int)y;
- (void)setScrollXOffset:(int)x scrollYOffset:(int)y adjustForIOSCaret:(BOOL)adjustForIOSCaret;
- (void)absolutePosition:(int*)x :(int*)y :(int*)w :(int*)h;
@end

typedef struct _WKQuad {
    CGPoint p1;
    CGPoint p2;
    CGPoint p3;
    CGPoint p4;
} WKQuad;

@interface WKQuadObject : NSObject
- (id)initWithQuad:(WKQuad)quad;
- (WKQuad)quad;
- (CGRect)boundingBox;
@end

#endif

@interface DOMNode (DOMNodeExtensions)
#if TARGET_OS_IPHONE
- (CGRect)boundingBox;
#else
- (NSRect)boundingBox WEBKIT_AVAILABLE_MAC(10_5);
#endif
- (NSArray *)lineBoxRects WEBKIT_AVAILABLE_MAC(10_5);

#if TARGET_OS_IPHONE
- (CGRect)boundingBoxUsingTransforms; // takes transforms into account

- (WKQuad)absoluteQuad;
- (WKQuad)absoluteQuadAndInsideFixedPosition:(BOOL *)insideFixed;
- (NSArray *)lineBoxQuads; // returns array of WKQuadObject

- (NSURL *)hrefURL;
- (CGRect)hrefFrame;
- (NSString *)hrefTarget;
- (NSString *)hrefLabel;
- (NSString *)hrefTitle;
- (CGRect)boundingFrame;
- (WKQuad)innerFrameQuad; // takes transforms into account
- (float)computedFontSize;
- (DOMNode *)nextFocusNode;
- (DOMNode *)previousFocusNode;
#endif
@end

@interface DOMElement (DOMElementAppKitExtensions)
#if !TARGET_OS_IPHONE
- (NSImage *)image WEBKIT_AVAILABLE_MAC(10_5);
#endif
@end

@interface DOMHTMLDocument (DOMHTMLDocumentExtensions)
- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL WEBKIT_AVAILABLE_MAC(10_5);
- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text WEBKIT_AVAILABLE_MAC(10_5);
@end

#if TARGET_OS_IPHONE

@interface DOMHTMLAreaElement (DOMHTMLAreaElementExtensions)
- (CGRect)boundingFrameForOwner:(DOMNode *)anOwner;
@end

@interface DOMHTMLSelectElement (DOMHTMLSelectElementExtensions)
- (DOMNode *)listItemAtIndex:(int)anIndex;
@end

#endif
