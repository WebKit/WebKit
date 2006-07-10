/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#import <WebCore/DOMCSS.h>
#import <WebCore/DOMHTML.h>
#import <WebCore/DOMRange.h>

@class NSColor;
@class NSImage;

@interface DOMImplementation (DOMImplementationExtensions)
- (DOMHTMLDocument *)createHTMLDocument:(NSString *)title;
@end

@interface DOMDocument (DOMDocumentCSSExtensions)
- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration;
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)elt :(NSString *)pseudoElt;
@end

@interface DOMHTMLDocument (DOMHTMLDocumentExtensions)
- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL;
- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text;
@end

@interface DOMHTMLElement (DOMHTMLElementExtensions)
- (NSString *)innerHTML;
- (void)setInnerHTML:(NSString *)innerHTML;
- (NSString *)innerText;
- (void)setInnerText:(NSString *)innerText;
- (NSString *)outerHTML;
- (void)setOuterHTML:(NSString *)outerHTML;
- (NSString *)outerText;
- (void)setOuterText:(NSString *)outerText;
- (DOMHTMLCollection *)children;
- (NSString *)contentEditable;
- (void)setContentEditable:(NSString *)contentEditable;
- (BOOL)isContentEditable;
- (NSString *)titleDisplayString;

- (int)offsetLeft;
- (int)offsetTop;
- (int)offsetWidth;
- (int)offsetHeight;
- (DOMHTMLElement *)offsetParent;

- (int)clientWidth;
- (int)clientHeight;

- (int)scrollLeft;
- (void)setScrollLeft:(int)scroll;
- (int)scrollTop;
- (void)setScrollTop:(int)scroll;
- (int)scrollWidth;
- (int)scrollHeight;
@end

@interface DOMHTMLEmbedElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (int)height;
- (void)setHeight:(int)height;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
- (NSString *)type;
- (void)setType:(NSString *)type;
- (int)width;
- (void)setWidth:(int)width;
@end

@interface DOMRGBColor (DOMRGBColorExtensions)
- (DOMCSSPrimitiveValue *)alpha;
- (NSColor *)color;
@end

@interface DOMAttr (DOMAttrExtensions)
- (DOMCSSStyleDeclaration *)style;
@end

@interface DOMCSSStyleDeclaration (DOMCSSStyleDeclarationExtensions)
- (NSString *)getPropertyShorthand:(NSString *)propertyName;
- (BOOL)isPropertyImplicit:(NSString *)propertyName;
@end

@interface DOMNode (DOMNodeExtensions)
- (NSRect)boundingBox;
- (NSArray *)lineBoxRects;
@end

@interface DOMRange (DOMRangeExtensions)
- (NSString *)text;
@end

@interface DOMElement (DOMElementExtensions)
- (NSImage *)image;
- (void)scrollIntoView:(BOOL)alignTop;
- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded;
@end

@interface DOMHTMLInputElement (DOMHTMLInputElementExtensions)
- (NSString *)altDisplayString;
- (NSURL *)absoluteImageURL;
@end

@interface DOMHTMLImageElement (DOMHTMLImageElementExtensions)
- (NSString *)altDisplayString;
- (NSURL *)absoluteImageURL;
@end

@interface DOMHTMLObjectElement (DOMHTMLObjectElementExtensions)
- (NSURL *)absoluteImageURL;
@end

@interface DOMHTMLAnchorElement (DOMHTMLAnchorElementExtensions)
- (NSURL *)absoluteLinkURL;
@end

@interface DOMHTMLAreaElement (DOMHTMLAreaElementExtensions)
- (NSURL *)absoluteLinkURL;
@end

@interface DOMHTMLLinkElement (DOMHTMLLinkElementExtensions)
- (NSURL *)absoluteLinkURL;
@end
