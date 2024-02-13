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
#import <PDFKit/PDFSelectionPriv.h>
#if __has_include(<PDFKit/PDFActionPriv.h>)
#import <PDFKit/PDFActionPriv.h>
#else
@interface PDFAction(SPI)
- (NSArray *) nextActions;
@end
#endif // __has_include(PDFKIT/PDFActionPriv.h)

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
- (/*nullable*/ NSData *)webArchive;
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

@interface PDFPage (IPI)
- (CGPDFPageLayoutRef) pageLayout;
@end
#endif // ENABLE(UNIFIED_PDF)
