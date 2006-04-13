/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

@interface DOMRange (WebPrivate)
// uses same algorithm as innerText
- (NSString *)_text;
@end

@interface DOMRGBColor (WebPrivate)
// fast and easy way of getting an NSColor for a DOMRGBColor
- (NSColor *)_color;
@end

@interface DOMElement (WebPrivate)
- (NSFont *)_font;
- (NSData *)_imageTIFFRepresentation;
- (NSURL *)_getURLAttribute:(NSString *)name;
@end

@interface DOMCSSStyleDeclaration (WebPrivate)
- (NSString *)_fontSizeDelta;
- (void)_setFontSizeDelta:(NSString *)fontSizeDelta;
@end

@interface DOMHTMLDocument (WebPrivate)
- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString;
- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text;
@end

// All the methods in this category are used by Safari forms autofill and should not be used for any other purpose.
// They are stopgap measures until we finish transitioning form controls to not use NSView. Each one should become
// replaceable by public DOM API, and when that happens Safari will switch to implementations using that public API,
// and these will be deleted.
@interface DOMHTMLInputElement(FormsAutoFillTransition)
- (BOOL)_isTextField;
- (NSRect)_rectOnScreen; // bounding box of the text field, in screen coordinates
- (void)_replaceCharactersInRange:(NSRange)targetRange withString:(NSString *)replacementString selectingFromIndex:(int)index;
- (NSRange)_selectedRange;
- (void)_setAutofilled:(BOOL)filled;
@end

// All the methods in this category are used by Safari forms autofill and should not be used for any other purpose.
// They are stopgap measures until we finish transitioning form controls to not use NSView. Each one should become
// replaceable by public DOM API, and when that happens Safari will switch to implementations using that public API,
// and these will be deleted.
@interface DOMHTMLSelectElement(FormsAutoFillTransition)
- (void)_activateItemAtIndex:(int)index;
@end

// BEGIN PENDING PUBLIC APIS
// These APIs will be made public eventually.
@interface DOMAttr (DOMAttrExtensions)
- (DOMCSSStyleDeclaration *)style;
@end

@interface DOMDocument (DOMViewCSSExtensions)
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)elt :(NSString *)pseudoElt;
@end

@interface DOMCSSStyleDeclaration (DOMCSSStyleDeclarationExtensions)
- (NSString *)getPropertyShorthand:(NSString *)propertyName;
- (BOOL)isPropertyImplicit:(NSString *)propertyName;
@end

@interface DOMNode (DOMNodePendingPublic)
- (BOOL)isSameNode:(DOMNode *) other;
- (BOOL)isEqualNode:(DOMNode *) other;

- (NSRect)boundingBox;
- (NSArray *)lineBoxRects;

- (NSString *)textContent;
- (void)setTextContent:(NSString *)text;

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (BOOL)dispatchEvent:(DOMEvent *)event;
@end

@interface DOMElement (DOMElementExtensions)
- (NSImage *)image;
- (void)focus;
- (void)blur;
- (void)scrollIntoView:(BOOL)alignTop;
- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded;
@end

@interface DOMHTMLElement (DOMHTMLElementPendingPublic)
- (NSString *)titleDisplayString;
@end

@interface DOMHTMLInputElement (DOMHTMLInputElementPendingPublic)
- (NSString *)altDisplayString;
- (NSURL *)absoluteImageURL;
@end

@interface DOMHTMLImageElement (DOMHTMLImageElementPendingPublic)
- (NSString *)altDisplayString;
- (NSURL *)absoluteImageURL;
@end

@interface DOMHTMLObjectElement (DOMHTMLObjectElementPendingPublic)
- (NSURL *)absoluteImageURL;
@end

@interface DOMHTMLAnchorElement (DOMHTMLAnchorElementPendingPublic)
- (NSURL *)absoluteLinkURL;
@end

@interface DOMHTMLAreaElement (DOMHTMLAreaElementPendingPublic)
- (NSURL *)absoluteLinkURL;
@end

@interface DOMHTMLLinkElement (DOMHTMLLinkElementPendingPublic)
- (NSURL *)absoluteLinkURL;
@end

// END

// Pending DOM3 APIs
@interface DOMDocument (DOM3)
- (DOMNode *)adoptNode:(DOMNode *)source;
@end
