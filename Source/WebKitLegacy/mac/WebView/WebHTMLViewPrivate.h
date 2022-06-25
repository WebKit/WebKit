/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2021 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import <WebKitLegacy/WebHTMLView.h>
#if TARGET_OS_IPHONE
#import <WebKitLegacy/WAKView.h>
#endif

@class DOMDocumentFragment;
@class DOMNode;
@class DOMRange;

extern const float _WebHTMLViewPrintingMinimumShrinkFactor;
extern const float _WebHTMLViewPrintingMaximumShrinkFactor;

@interface WebHTMLView (WebPrivate)

+ (NSArray *)supportedMIMETypes;
+ (NSArray *)supportedMediaMIMETypes;
+ (NSArray *)supportedImageMIMETypes;
+ (NSArray *)supportedNonImageMIMETypes;
+ (NSArray *)unsupportedTextMIMETypes;

- (void)close;

#if !TARGET_OS_IPHONE

// Modifier (flagsChanged) tracking SPI
+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent;
- (void)_updateMouseoverWithFakeEvent;

- (void)_setAsideSubviews;
- (void)_restoreSubviews;

- (void)_updateMouseoverWithEvent:(NSEvent *)event;

+ (NSArray *)_insertablePasteboardTypes;
+ (NSString *)_dummyPasteboardType;
+ (NSArray *)_selectionPasteboardTypes;
- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard;

#endif

- (void)_frameOrBoundsChanged;

#if !TARGET_OS_IPHONE
- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event;
#endif

// FIXME: _selectionRect is deprecated in favor of selectionRect, which is in protocol WebDocumentSelection.
// We can't remove this yet because it's still in use by Mail.
- (NSRect)_selectionRect;

- (BOOL)_canEdit;
- (BOOL)_canEditRichly;
- (BOOL)_canAlterCurrentSelection;
- (BOOL)_hasSelection;
- (BOOL)_hasSelectionOrInsertionPoint;
- (BOOL)_isEditable;

#if !TARGET_OS_IPHONE

- (BOOL)_transparentBackground;
- (void)_setTransparentBackground:(BOOL)isBackgroundTransparent;

#endif

- (void)_setToolTip:(NSString *)string;

#if !TARGET_OS_IPHONE

// SPI used by Mail.
// FIXME: These should all be moved to WebView; we won't always have a WebHTMLView.
- (NSImage *)_selectionDraggingImage;
- (NSRect)_selectionDraggingRect;
- (DOMNode *)_insertOrderedList;
- (DOMNode *)_insertUnorderedList;
- (BOOL)_canIncreaseSelectionListLevel;
- (BOOL)_canDecreaseSelectionListLevel;
- (DOMNode *)_increaseSelectionListLevel;
- (DOMNode *)_increaseSelectionListLevelOrdered;
- (DOMNode *)_increaseSelectionListLevelUnordered;
- (void)_decreaseSelectionListLevel;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard forType:(NSString *)pboardType inContext:(DOMRange *)context subresources:(NSArray **)subresources;

#endif

- (BOOL)_isUsingAcceleratedCompositing;
#if TARGET_OS_IPHONE
- (WAKView *)_compositingLayersHostingView;
#else
- (NSView *)_compositingLayersHostingView;
#endif

#if !TARGET_OS_IPHONE
// SPI for printing (should be converted to API someday). When the WebHTMLView isn't being printed
// directly, this method must be called before paginating, or the computed height might be incorrect.
// Typically this would be called from inside an override of -[NSView knowsPageRange:].
- (void)_layoutForPrinting;
#endif
- (CGFloat)_adjustedBottomOfPageWithTop:(CGFloat)top bottom:(CGFloat)bottom limit:(CGFloat)bottomLimit;
- (BOOL)_isInPrintMode;
- (BOOL)_beginPrintModeWithPageWidth:(float)pageWidth height:(float)pageHeight shrinkToFit:(BOOL)shrinkToFit;
// Lays out to pages of the given minimum width and height or more (increasing both dimensions proportionally)
// as needed for the content to fit, but no more than the given maximum width.
- (BOOL)_beginPrintModeWithMinimumPageWidth:(CGFloat)minimumPageWidth height:(CGFloat)minimumPageHeight maximumPageWidth:(CGFloat)maximumPageWidth;
- (void)_endPrintMode;

- (BOOL)_isInScreenPaginationMode;
- (BOOL)_beginScreenPaginationModeWithPageSize:(CGSize)pageSize shrinkToFit:(BOOL)shrinkToFit;
- (void)_endScreenPaginationMode;

#if !TARGET_OS_IPHONE
- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard;
#endif

#if TARGET_OS_IPHONE
- (id)accessibilityRootElement;
#endif

#if !TARGET_OS_IPHONE
- (NSView *)_hitViewForEvent:(NSEvent *)event;
#endif

@end
