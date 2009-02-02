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

#import <WebCore/DOMCSS.h>
#import <WebCore/DOMCSSStyleDeclaration.h>
#import <WebCore/DOMElement.h>
#import <WebCore/DOMEvents.h>
#import <WebCore/DOMHTML.h>
#import <WebCore/DOMHTMLDocument.h>
#import <WebCore/DOMHTMLInputElement.h>
#import <WebCore/DOMHTMLSelectElement.h>
#import <WebCore/DOMNode.h>
#import <WebCore/DOMRGBColor.h>
#import <WebCore/DOMRange.h>

#import <WebCore/DOMDocumentPrivate.h>
#import <WebCore/DOMElementPrivate.h>
#import <WebCore/DOMHTMLAnchorElementPrivate.h>
#import <WebCore/DOMHTMLAreaElementPrivate.h>
#import <WebCore/DOMHTMLBodyElementPrivate.h>
#import <WebCore/DOMHTMLButtonElementPrivate.h>
#import <WebCore/DOMHTMLDocumentPrivate.h>
#import <WebCore/DOMHTMLFormElementPrivate.h>
#import <WebCore/DOMHTMLFrameElementPrivate.h>
#import <WebCore/DOMHTMLImageElementPrivate.h>
#import <WebCore/DOMHTMLInputElementPrivate.h>
#import <WebCore/DOMHTMLLinkElementPrivate.h>
#import <WebCore/DOMHTMLOptionsCollectionPrivate.h>
#import <WebCore/DOMHTMLPreElementPrivate.h>
#import <WebCore/DOMHTMLStyleElementPrivate.h>
#import <WebCore/DOMHTMLTextAreaElementPrivate.h>
#import <WebCore/DOMKeyboardEventPrivate.h>
#import <WebCore/DOMMouseEventPrivate.h>
#import <WebCore/DOMNodeIteratorPrivate.h>
#import <WebCore/DOMNodePrivate.h>
#import <WebCore/DOMProcessingInstructionPrivate.h>
#import <WebCore/DOMRangePrivate.h>
#import <WebCore/DOMUIEventPrivate.h>
#import <WebCore/DOMWheelEventPrivate.h>

@interface DOMNode (DOMNodeExtensionsPendingPublic)
- (NSImage *)renderedImage;
@end

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSColor *)color.
@interface DOMRGBColor (WebPrivate)
- (NSColor *)_color;
@end

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSString *)text.
@interface DOMRange (WebPrivate)
- (NSString *)_text;
@end

@interface DOMRange (DOMRangeExtensions)
- (NSRect)boundingBox;
- (NSArray *)lineBoxRects;
@end

@interface DOMElement (WebPrivate)
- (NSFont *)_font;
- (NSData *)_imageTIFFRepresentation;
- (NSURL *)_getURLAttribute:(NSString *)name;
- (BOOL)isFocused;
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
@interface DOMHTMLInputElement (FormsAutoFillTransition)
- (BOOL)_isTextField;
- (NSRect)_rectOnScreen; // bounding box of the text field, in screen coordinates
- (void)_replaceCharactersInRange:(NSRange)targetRange withString:(NSString *)replacementString selectingFromIndex:(int)index;
- (NSRange)_selectedRange;
- (void)_setAutofilled:(BOOL)filled;
@end

// These changes are necessary to detect whether a form input was modified by a user
// or javascript
@interface DOMHTMLInputElement (FormPromptAdditions)
- (BOOL)_isEdited;
@end

@interface DOMHTMLTextAreaElement (FormPromptAdditions)
- (BOOL)_isEdited;
@end

// All the methods in this category are used by Safari forms autofill and should not be used for any other purpose.
// They are stopgap measures until we finish transitioning form controls to not use NSView. Each one should become
// replaceable by public DOM API, and when that happens Safari will switch to implementations using that public API,
// and these will be deleted.
@interface DOMHTMLSelectElement (FormsAutoFillTransition)
- (void)_activateItemAtIndex:(int)index;
@end
