/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#if TARGET_OS_IPHONE

#import <WebKitLegacy/DOMElement.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/DOMHTMLAreaElement.h>
#import <WebKitLegacy/DOMHTMLImageElement.h>
#import <WebKitLegacy/DOMHTMLSelectElement.h>
#import <WebKitLegacy/DOMNode.h>
#import <WebKitLegacy/DOMRange.h>

typedef enum { 
    // The first four match SelectionDirection.  The last two don't have WebKit counterparts because
    // they aren't necessary until there is support vertical layout.
    WebTextAdjustmentForward,
    WebTextAdjustmentBackward,
    WebTextAdjustmentRight,
    WebTextAdjustmentLeft,
    WebTextAdjustmentUp,
    WebTextAdjustmentDown
} WebTextAdjustmentDirection; 

@interface DOMRange (UIKitExtensions)

- (void)move:(UInt32)amount inDirection:(WebTextAdjustmentDirection)direction;
- (void)extend:(UInt32)amount inDirection:(WebTextAdjustmentDirection)direction;
- (DOMNode *)firstNode;

@end

@interface DOMNode (UIKitExtensions)
- (NSArray *)borderRadii;
- (NSArray *)boundingBoxes;
- (NSArray *)absoluteQuads;     // return array of WKQuadObjects. takes transforms into account

- (BOOL)containsOnlyInlineObjects;
- (BOOL)isSelectableBlock;
- (DOMRange *)rangeOfContainingParagraph;
- (CGFloat)textHeight;
- (DOMNode *)findExplodedTextNodeAtPoint:(CGPoint)point;  // A second-chance pass to look for text nodes missed by the hit test.
@end

@interface DOMHTMLAreaElement (UIKitExtensions)
- (CGRect)boundingBoxWithOwner:(DOMNode *)anOwner;
- (WKQuad)absoluteQuadWithOwner:(DOMNode *)anOwner;     // takes transforms into account
- (NSArray *)boundingBoxesWithOwner:(DOMNode *)anOwner;
- (NSArray *)absoluteQuadsWithOwner:(DOMNode *)anOwner; // return array of WKQuadObjects. takes transforms into account
@end

@interface DOMHTMLSelectElement (UIKitExtensions)
- (unsigned)completeLength;
- (DOMNode *)listItemAtIndex:(int)anIndex;
@end

@interface DOMHTMLImageElement (WebDOMHTMLImageElementOperationsPrivate)
- (NSData *)dataRepresentation:(BOOL)rawImageData;
- (NSString *)mimeType;
@end

@interface DOMElement (DOMUIKitComplexityExtensions) 
- (int)structuralComplexityContribution; // Does not include children.
@end

#endif // TARGET_OS_IPHONE
