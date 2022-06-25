/*
 * Copyright (C) 2004-2006 Apple Inc.  All rights reserved.
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

#import <WebKitLegacy/DOM.h>
#import <WebKitLegacy/WebAutocapitalizeTypes.h>
#import <WebKitLegacy/WebDOMOperationsPrivate.h>

#if TARGET_OS_IPHONE
#import <CoreText/CoreText.h>
#endif

@interface DOMNode (DOMNodeExtensionsPendingPublic)
#if !TARGET_OS_IPHONE
- (NSImage *)renderedImage;
#endif
- (NSArray *)textRects;
@end

@interface DOMNode (WebPrivate)
+ (id)_nodeFromJSWrapper:(JSObjectRef)jsWrapper;
- (void)getPreviewSnapshotImage:(CGImageRef*)cgImage andRects:(NSArray **)rects;
@end

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSColor *)color.
@interface DOMRGBColor (WebPrivate)
#if !TARGET_OS_IPHONE
- (NSColor *)_color;
#endif
@end

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSString *)text.
@interface DOMRange (WebPrivate)
- (NSString *)_text;
@end

@interface DOMRange (DOMRangeExtensions)
#if TARGET_OS_IPHONE
- (CGRect)boundingBox;
#else
- (NSRect)boundingBox;
#endif
#if !TARGET_OS_IPHONE
- (NSImage *)renderedImageForcingBlackText:(BOOL)forceBlackText;
#else
- (CGImageRef)renderedImageForcingBlackText:(BOOL)forceBlackText;
#endif
- (NSArray *)lineBoxRects; // Deprecated. Use textRects instead.
- (NSArray *)textRects;
@end

@interface DOMElement (WebPrivate)
#if !TARGET_OS_IPHONE
- (NSData *)_imageTIFFRepresentation;
#endif
- (CTFontRef)_font;
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

@interface DOMHTMLTableCellElement (WebPrivate)
- (DOMHTMLTableCellElement *)_cellAbove;
@end

@interface DOMHTMLInputElement (FormAutoFillTransition)
- (BOOL)_isTextField;
@end

#if TARGET_OS_IPHONE
// These changes are necessary to detect whether a form input was modified by a user
// or javascript
@interface DOMHTMLInputElement (FormPromptAdditions)
- (BOOL)_isEdited;
@end

@interface DOMHTMLTextAreaElement (FormPromptAdditions)
- (BOOL)_isEdited;
@end
#endif // TARGET_OS_IPHONE

@interface DOMHTMLSelectElement (FormAutoFillTransition)
- (void)_activateItemAtIndex:(int)index;
- (void)_activateItemAtIndex:(int)index allowMultipleSelection:(BOOL)allowMultipleSelection;
@end

#if TARGET_OS_IPHONE
enum { WebMediaQueryOrientationCurrent, WebMediaQueryOrientationPortrait, WebMediaQueryOrientationLandscape };
@interface DOMHTMLLinkElement (WebPrivate)
- (BOOL)_mediaQueryMatchesForOrientation:(int)orientation;
- (BOOL)_mediaQueryMatches;
@end

// These changes are useful to get the AutocapitalizeType on particular form controls.
@interface DOMHTMLInputElement (AutocapitalizeAdditions)
- (WebAutocapitalizeType)_autocapitalizeType;
@end

@interface DOMHTMLTextAreaElement (AutocapitalizeAdditions)
- (WebAutocapitalizeType)_autocapitalizeType;
@end

// These are used by Date and Time input types because the generated ObjC methods default to not dispatching events.
@interface DOMHTMLInputElement (WebInputChangeEventAdditions)
- (void)setValueWithChangeEvent:(NSString *)newValue;
- (void)setValueAsNumberWithChangeEvent:(double)newValueAsNumber;
@end
#endif // TARGET_OS_IPHONE
