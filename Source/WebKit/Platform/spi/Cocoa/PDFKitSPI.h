/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#import <PDFKit/PDFKit.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#if HAVE(PDFKIT)

#if PLATFORM(IOS_FAMILY)
#import <PDFKit/PDFHostViewController.h>
#endif // PLATFORM(IOS_FAMILY)

#import <PDFKit/PDFDocumentPriv.h>
#import <PDFKit/PDFPagePriv.h>
#import <PDFKit/PDFSelectionPriv.h>

#if HAVE(PDFKIT_WITH_NEXT_ACTIONS)
#import <PDFKit/PDFActionPriv.h>
#endif

#endif // HAVE(PDFKIT)

#else

#if HAVE(PDFKIT)

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"

@interface _UIRemoteViewController : UIViewController
@end

@protocol PDFHostViewControllerDelegate<NSObject>
@end

@interface PDFHostViewController : _UIRemoteViewController<UIGestureRecognizerDelegate, UIDocumentPasswordViewDelegate>

@property (nonatomic, class) bool useIOSurfaceForTiles;

+ (void) createHostView:(void(^)(PDFHostViewController* hostViewController)) callback forExtensionIdentifier:(NSString*) extensionIdentifier;
- (void) setDelegate:(id<PDFHostViewControllerDelegate>) delegate;
- (void) setDocumentData:(NSData*) data withScrollView:(UIScrollView*) scrollView;

- (void) findString:(NSString*) string withOptions:(NSStringCompareOptions) options;
- (void) cancelFindString;
- (void) cancelFindStringWithHighlightsCleared:(BOOL)cleared;
- (void) focusOnSearchResultAtIndex:(NSUInteger) searchIndex;

- (NSInteger) currentPageIndex;
- (NSInteger) pageCount;
- (UIView*) pageNumberIndicator;
- (void) goToPageIndex:(NSInteger) pageIndex;
- (void) updatePDFViewLayout;

+ (UIColor *)backgroundColor;

- (void) beginPDFViewRotation;
- (void) endPDFViewRotation;

- (void) snapshotViewRect: (CGRect) rect snapshotWidth: (NSNumber*) width afterScreenUpdates: (BOOL) afterScreenUpdates withResult: (void (^)(UIImage* image)) completion;

@end
#endif // PLATFORM(IOS_FAMILY)

@interface PDFSelection (SPI)
- (void)drawForPage:(PDFPage *)page withBox:(CGPDFBox)box active:(BOOL)active inContext:(CGContextRef)context;
- (PDFPoint)firstCharCenter;
- (/*nullable*/ NSString *)html;
- (NSAttributedString *)attributedStringScaled:(CGFloat)scale;
- (BOOL)isEmpty;
@end

#if HAVE(PDFDOCUMENT_ANNOTATIONS_FOR_FIELD_NAME)
@interface PDFDocument (PDFDocumentPriv)
- (NSArray *)annotationsForFieldName:(NSString *)fieldname;
@end
#endif

@interface PDFAction (PDFActionPriv)
- (NSArray *)nextActions;
@end

#endif // HAVE(PDFKIT)

#endif // USE(APPLE_INTERNAL_SDK)

#if HAVE(INCREMENTAL_PDF_APIS)
@interface PDFDocument ()
-(instancetype)initWithProvider:(CGDataProviderRef)dataProvider;
-(void)preloadDataOfPagesInRange:(NSRange)range onQueue:(dispatch_queue_t)queue completion:(void (^)(NSIndexSet* loadedPageIndexes))completionBlock;
-(void)resetFormFields:(PDFActionResetForm *) action;
@property (readwrite, nonatomic) BOOL hasHighLatencyDataProvider;
@end
#endif // HAVE(INCREMENTAL_PDF_APIS)

#if ENABLE(UNIFIED_PDF)
@interface PDFDocument (IPI)
- (PDFDestination *)namedDestination:(NSString *)name;
@end

#if HAVE(COREGRAPHICS_WITH_PDF_AREA_OF_INTEREST_SUPPORT)
@interface PDFPage (IPI)
- (CGPDFPageLayoutRef) pageLayout;
@end
#endif

#if HAVE(PDFPAGE_AREA_OF_INTEREST_AT_POINT)
#define PDFAreaOfInterest NSInteger

#define kPDFTextArea        (1UL << 1)
#define kPDFAnnotationArea  (1UL << 2)
#define kPDFLinkArea        (1UL << 3)
#define kPDFControlArea     (1UL << 4)
#define kPDFTextFieldArea   (1UL << 5)
#define kPDFIconArea        (1UL << 6)
#define kPDFPopupArea       (1UL << 7)
#define kPDFImageArea       (1UL << 8)

@interface PDFPage (Staging_119217538)
- (PDFAreaOfInterest)areaOfInterestAtPoint:(PDFPoint)point;
@end
#endif

#if HAVE(PDFDOCUMENT_SELECTION_WITH_GRANULARITY)
typedef NS_ENUM(NSUInteger, PDFSelectionGranularity);

#define PDFSelectionGranularityCharacter 0
#define PDFSelectionGranularityWord 1
#define PDFSelectionGranularityLine 2

@interface PDFDocument (Staging_122179178)
- (/*nullable*/ PDFSelection *)selectionFromPage:(PDFPage *)startPage atPoint:(PDFPoint)startPoint toPage:(PDFPage *)endPage atPoint:(PDFPoint)endPoint withGranularity:(PDFSelectionGranularity)granularity;
@end
#endif

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

#if HAVE(PDFDOCUMENT_ENABLE_DATA_DETECTORS)
@interface PDFDocument (Staging_123761050)
@property (nonatomic) BOOL enableDataDetectors;
@end
#endif

#if HAVE(PDFPAGE_DATA_DETECTOR_RESULTS)
@interface PDFPage (Staging_123761050)
- (NSArray *)dataDetectorResults;
@end
#endif

#endif

#if HAVE(PDFSELECTION_ENUMERATE_RECTS_AND_TRANSFORMS)

@interface PDFSelection (Staging_125426369)
- (void)enumerateRectsAndTransformsForPage:(PDFPage *)page usingBlock:(void (^)(CGRect rect, CGAffineTransform transform))block;
@end

#endif

#if HAVE(PDFSELECTION_HTMLDATA_RTFDATA)

@interface PDFSelection (Staging_136075998)
- (/*nullable*/ NSData *)htmlData;
- (/*nullable*/ NSData *)rtfData;
@end

#endif

#endif // ENABLE(UNIFIED_PDF)

// FIXME: Move this declaration inside the !USE(APPLE_INTERNAL_SDK) block once rdar://problem/118903435 is in builds.
@interface PDFDocument (AX)
- (NSArray *)accessibilityChildren:(id)parent;
@end
