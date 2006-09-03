/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
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

#import <WebCore/DOMAttr.h>
#import <WebCore/DOMCSS.h>
#import <WebCore/DOMCSSStyleDeclaration.h>
#import <WebCore/DOMDOMImplementation.h>
#import <WebCore/DOMDocument.h>
#import <WebCore/DOMElement.h>
#import <WebCore/DOMHTML.h>
#import <WebCore/DOMHTMLAnchorElement.h>
#import <WebCore/DOMHTMLAreaElement.h>
#import <WebCore/DOMHTMLDocument.h>
#import <WebCore/DOMHTMLElement.h>
#import <WebCore/DOMHTMLImageElement.h>
#import <WebCore/DOMHTMLInputElement.h>
#import <WebCore/DOMHTMLLinkElement.h>
#import <WebCore/DOMHTMLObjectElement.h>
#import <WebCore/DOMNode.h>
#import <WebCore/DOMRGBColor.h>
#import <WebCore/DOMRange.h>

@class NSImage;

@interface DOMDocument (DOMDocumentCSSExtensions)
- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration;
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)elt :(NSString *)pseudoElt;
@end

@interface DOMElement (DOMElementAppKitExtensions)
- (NSImage *)image;
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

@interface DOMCSSStyleDeclaration (DOMCSSStyleDeclarationExtensions)
- (NSString *)getPropertyShorthand:(NSString *)propertyName;
- (BOOL)isPropertyImplicit:(NSString *)propertyName;
@end

@interface DOMRange (DOMRangeExtensions)
- (NSString *)text;
@end
