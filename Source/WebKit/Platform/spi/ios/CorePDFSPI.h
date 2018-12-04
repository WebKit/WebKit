/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(WKLEGACYPDFVIEW) || ENABLE(WKPDFVIEW)

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <CorePDF/UIPDFAnnotationController.h>
#import <CorePDF/UIPDFDocument.h>
#import <CorePDF/UIPDFLinkAnnotation.h>
#import <CorePDF/UIPDFPage.h>
#import <CorePDF/UIPDFPageView.h>
#import <CorePDF/UIPDFSelection.h>

#else

@class UIPDFSelection;

@interface UIPDFPage : NSObject
@end

@interface UIPDFPage ()
- (CGRect)cropBoxAccountForRotation;
- (UIPDFSelection *)findString:(NSString *)string fromSelection:(UIPDFSelection *)selection options:(NSStringCompareOptions)options;
@end

@interface UIPDFDocument : NSObject
@end

@interface UIPDFDocument ()
- (UIPDFPage *)pageAtIndex:(NSUInteger)index;
- (id)initWithCGPDFDocument:(CGPDFDocumentRef)document;
@property (assign, readonly) NSUInteger numberOfPages;
@property (readonly) CGPDFDocumentRef CGDocument;
@end

typedef enum {
    kUIPDFObjectKindGraphic = 1,
    kUIPDFObjectKindText = 2
} UIPDFObjectKind;

@class UIPDFPageView;
@protocol UIPDFAnnotationControllerDelegate;

@interface UIPDFAnnotationController : NSObject<UIGestureRecognizerDelegate>
@end

@interface UIPDFAnnotationController ()
@property (nonatomic, readonly) UIPDFPageView *pageView;
@property (nonatomic, assign) id<NSObject, UIPDFAnnotationControllerDelegate> delegate;
@end

@protocol UIPDFPageViewDelegate;

@interface UIPDFPageView : UIView
@end

@interface UIPDFPageView ()
- (id)initWithPage:(UIPDFPage *) page tiledContent:(BOOL)tiled;
- (CGRect)convertRectFromPDFPageSpace:(CGRect)p;
- (void)highlightSearchSelection:(UIPDFSelection *)selection animated:(BOOL)animated;
- (void)clearSearchHighlights;
@property (nonatomic, assign) BOOL useBackingLayer;
@property (nonatomic, assign) id<NSObject, UIPDFPageViewDelegate> delegate;
@property (nonatomic, readonly) CALayer *contentLayer;
@property (nonatomic, readonly) UIPDFAnnotationController *annotationController;
@end

@protocol UIPDFPageViewDelegate
@optional
- (BOOL)selectionWillTrack:(UIPDFPageView*)pageView;
- (BOOL)shouldRecognizeTapIn:(UIPDFPageView *)pageView atPoint:(CGPoint)point;
- (Class)classForAnnotationType:(const char *)type;
- (void)didTap:(UIPDFPageView *)pageView atPoint:(CGPoint)point;
- (void)doubleTapIn:(UIPDFPageView *)pageView atPoint:(CGPoint)point;
- (void)pageWasRendered:(UIPDFPageView *)pageView;
- (void)resetZoom:(UIPDFPageView *)pageView;
- (void)selectionDidEndTracking:(UIPDFPageView *)pageView;
- (void)zoom:(UIPDFPageView *)pageView to:(CGRect)rect atPoint:(CGPoint)pt kind:(UIPDFObjectKind)kind;
@end

@interface UIPDFAnnotation : NSObject
@end

@interface UIPDFAnnotation ()
- (CGRect)Rect;
@property (nonatomic, assign) UIPDFAnnotationController* annotationController;
@end

@interface UIPDFMarkupAnnotation : UIPDFAnnotation
@end

@interface UIPDFLinkAnnotation : UIPDFMarkupAnnotation
@end

@interface UIPDFLinkAnnotation ()
- (NSURL *)url;
- (NSUInteger)pageNumber;
@end

@protocol UIPDFAnnotationControllerDelegate
@optional
- (void)annotation:(UIPDFAnnotation *)annotation wasTouchedAtPoint:(CGPoint) point controller:(UIPDFAnnotationController *)controller;
- (void)annotation:(UIPDFAnnotation *)annotation isBeingPressedAtPoint:(CGPoint) point controller:(UIPDFAnnotationController *)controller;
@end

@interface UIPDFSelection : NSObject
@end

@interface UIPDFSelection ()
- (id)initWithPage:(UIPDFPage *)page fromIndex:(NSUInteger)startIndex toIndex:(NSUInteger)endIndex;
- (CGRect)bounds;
- (UIPDFPage *)page;
- (NSUInteger)startIndex;
@property (nonatomic, assign) CFRange stringRange;
@end

#endif

#endif // ENABLE(WKLEGACYPDFVIEW) || ENABLE(WKPDFVIEW)
