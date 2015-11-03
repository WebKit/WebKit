/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PDFLayerControllerSPI_h
#define PDFLayerControllerSPI_h

#include "PDFKitImports.h"

#if ENABLE(PDFKIT_PLUGIN)
#if USE(DEPRECATED_PDF_PLUGIN)

#include "DeprecatedPDFLayerControllerSPI.h"

#else // USE(DEPRECATED_PDF_PLUGIN)

#import <PDFKit/PDFKit.h>

@class CPReadingModel;
@class PDFViewLayout;

@protocol PDFLayerControllerDelegate <NSObject>

@optional

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController scrollToPoint:(CGPoint)newPosition;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController invalidateRect:(CGRect)rect;
- (void)pdfLayerControllerInvalidateHUD:(PDFLayerController *)pdfLayerController;

- (void)pdfLayerControllerZoomIn:(PDFLayerController *)pdfLayerController;
- (void)pdfLayerControllerZoomOut:(PDFLayerController *)pdfLayerController;

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeActiveAnnotation:(PDFAnnotation *)annotation;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didClickLinkWithURL:(NSURL *)url;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeDisplayMode:(int)mode;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeSelection:(PDFSelection *)selection;

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController copyItems:(NSArray *)items withTypes:(NSArray *)types;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController showDefinitionForAttributedString:(NSAttributedString *)string atPoint:(CGPoint)point;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController performWebSearchForString:(NSString *)string;
- (void)pdfLayerController:(PDFLayerController *)pdfLayerController performSpotlightSearchForString:(NSString *)string;
- (void)pdfLayerControllerSaveToPDF:(PDFLayerController *)pdfLayerController;
- (void)pdfLayerControllerOpenWithNativeApplication:(PDFLayerController *)pdfLayerController;

- (NSColorSpace*)pdfLayerControllerColorSpace:(PDFLayerController *)pdfLayerController;

@end

@interface PDFLayerController : NSObject
@end

@interface PDFLayerController ()

@property (retain) PDFDocument *document;
@property (assign) id<PDFLayerControllerDelegate> delegate;
@property (retain) NSArray *searchMatches;

- (void)setFrameSize:(CGSize)size;

- (void)setDisplayMode:(int)mode;
- (int)displayMode;
- (int)realDisplayMode;

- (CGSize)contentSize;
- (CGSize)contentSizeRespectingZoom;

- (CGFloat)contentScaleFactor;
- (void)setContentScaleFactor:(CGFloat)contentScaleFactor;

- (PDFViewLayout *)layout;

- (void)drawInContext:(CGContextRef)context;
- (void)drawHUDInContext:(CGContextRef)context;

- (void)mouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;

- (BOOL)mouseDown:(NSEvent *)event inHUDWithBounds:(CGRect)bounds;
- (BOOL)mouseUp:(NSEvent *)event inHUDWithBounds:(CGRect)bounds;
- (BOOL)mouseDragged:(NSEvent *)event inHUDWithBounds:(CGRect)bounds;

- (NSMenu *)menuForEvent:(NSEvent *)event;

- (NSArray *)pageRects;

- (void)setVisibleRect:(CGRect)visibleRect;

- (void)gotoSelection:(PDFSelection *)selection;
- (void)gotoDestination:(PDFDestination *)destination;
- (void)gotoRect:(CGRect)rect onPage:(PDFPage *)page;

- (PDFSelection *)currentSelection;
- (void)setCurrentSelection:(PDFSelection *)selection;

- (void)searchInDictionaryWithSelection:(PDFSelection *)selection;
- (PDFSelection *)getSelectionForWordAtPoint:(CGPoint)point;
- (NSArray *)rectsForSelectionInLayoutSpace:(PDFSelection *)selection;

- (NSArray *)highlights;
- (void)setHighlights:(NSArray*)highlights;

- (PDFSelection *)searchSelection;
- (NSArray *)searchMatches;
- (void)setSearchSelection:(PDFSelection*)selection;
- (NSArray *)findString:(NSString *)string caseSensitive:(BOOL)isCaseSensitive highlightMatches:(BOOL)shouldHighlightMatches;

- (void)copySelection;
- (void)selectAll;

- (PDFPage *)currentPage;
- (NSUInteger)currentPageIndex;

- (BOOL)documentIsLocked;
- (void)attemptToUnlockWithPassword:(NSString *)password;

- (CGRect)boundsForAnnotation:(PDFAnnotation *)annotation;
- (void)activateNextAnnotation:(BOOL)previous;

- (NSRect)frame;

// Accessibility

- (CPReadingModel *)readingModel;
- (id)accessibilityFocusedUIElement;
- (NSArray *)accessibilityAttributeNames;
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute;
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute;
- (NSArray *)accessibilityParameterizedAttributeNames;
- (NSString *)accessibilityRoleAttribute;
- (NSString *)accessibilityRoleDescriptionAttribute;
- (NSString *)accessibilityValueAttribute;
- (BOOL)accessibilityIsValueAttributeSettable;
- (NSString *)accessibilitySelectedTextAttribute;
- (BOOL)accessibilityIsSelectedTextAttributeSettable;
- (NSValue *)accessibilitySelectedTextRangeAttribute;
- (NSNumber *)accessibilityNumberOfCharactersAttribute;
- (BOOL)accessibilityIsNumberOfCharactersAttributeSettable;
- (NSValue *)accessibilityVisibleCharacterRangeAttribute;
- (BOOL)accessibilityIsVisibleCharacterRangeAttributeSettable;
- (NSNumber *)accessibilityLineForIndexAttributeForParameter:(id)parameter;
- (NSValue *)accessibilityRangeForLineAttributeForParameter:(id)parameter;
- (NSString *)accessibilityStringForRangeAttributeForParameter:(id)parameter;
- (NSValue *)accessibilityBoundsForRangeAttributeForParameter:(id)parameter;

@end

#endif // USE(DEPRECATED_PDF_PLUGIN)
#endif // ENABLE(PDFPLUGIN)

#endif // PDFLayerControllerSPI_h
