/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WebPDFViewIOS.h"
#import "WebDataSourceInternal.h"

#import "WebFrameInternal.h"
#import "WebJSPDFDoc.h"
#import "WebKitVersionChecks.h"
#import "WebPDFDocumentExtras.h"
#import "WebPDFViewPlaceholder.h"
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <WebCore/Color.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/StringWithDirection.h>
#import <WebCore/WKGraphics.h>
#import <WebKitLegacy/WebFrame.h>
#import <WebKitLegacy/WebFrameLoadDelegate.h>
#import <WebKitLegacy/WebFramePrivate.h>
#import <WebKitLegacy/WebFrameView.h>
#import <WebKitLegacy/WebNSViewExtras.h>
#import <WebKitLegacy/WebViewPrivate.h>
#import <wtf/Assertions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;
using namespace std;

static int comparePageRects(const void *key, const void *array);

static const float PAGE_WIDTH_INSET     = 4.0 * 2.0;
static const float PAGE_HEIGHT_INSET    = 4.0 * 2.0;

static RetainPtr<CGColorRef> createCGColorWithDeviceWhite(CGFloat white, CGFloat alpha)
{
    static NeverDestroyed<RetainPtr<CGColorSpaceRef>> graySpace = adoptCF(CGColorSpaceCreateDeviceGray());
    const CGFloat components[] = { white, alpha };
    return adoptCF(CGColorCreate(graySpace.get().get(), components));
}

@implementation WebPDFView {
    BOOL dataSourceHasBeenSet;
    CGPDFDocumentRef _PDFDocument;
    RetainPtr<NSString> _title;
    CGRect *_pageRects;
}

+ (NSArray *)supportedMIMETypes
{
    return @[
        @"text/pdf",
        @"application/pdf",
    ];
}

+ (CGColorRef)shadowColor
{
    static NeverDestroyed<RetainPtr<CGColorRef>> shadowColor = createCGColorWithDeviceWhite(0, 2.0 / 3);
    return shadowColor.get().get();
}

+ (CGColorRef)backgroundColor
{
    static NeverDestroyed<RetainPtr<CGColorRef>> backgroundColor = createCGColorWithDeviceWhite(204.0 / 255, 1);
    return backgroundColor.get().get();
}

// This is a secret protocol for WebDataSource and WebFrameView to offer us the opportunity to do something different 
// depending upon which frame is trying to instantiate a representation for PDF.
+ (Class)_representationClassForWebFrame:(WebFrame *)webFrame
{
    // If this is not the main frame, use the old WAK PDF view.
    if ([[webFrame webView] mainFrame] != webFrame)
        return [WebPDFView class];
    
    return [WebPDFViewPlaceholder class];
}

- (void)dealloc
{
    if (_PDFDocument != NULL)
        CGPDFDocumentRelease(_PDFDocument);
    free(_pageRects);
    [super dealloc];
}

- (void)drawPage:(CGPDFPageRef)aPage
{
    CGContextRef context = WKGetCurrentGraphicsContext();
    size_t pageNumber = CGPDFPageGetPageNumber(aPage);
    CGRect pageRect = _pageRects[pageNumber-1];
    
    // Draw page.
    CGContextSaveGState(context);
    CGContextSetShadowWithColor(context, CGSizeMake(0.0f, 2.0f), 3.0f, [[self class] shadowColor]);
    CGContextSetFillColorWithColor(context, cachedCGColor(Color::white).get());
    CGContextFillRect(context, pageRect);
    CGContextRestoreGState(context);    
    
    CGContextSaveGState(context);

    CGContextTranslateCTM(context, CGRectGetMinX(pageRect), CGRectGetMinY(pageRect));
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextTranslateCTM(context, 0, -CGRectGetHeight(pageRect));

    CGContextConcatCTM(context, CGPDFPageGetDrawingTransform(aPage, kCGPDFCropBox, CGRectMake(0.0, 0.0, CGRectGetWidth(pageRect), CGRectGetHeight(pageRect)), 0, true));
    
    CGRect cropBox = CGPDFPageGetBoxRect(aPage, kCGPDFCropBox);
    CGContextClipToRect(context, cropBox);
    
    CGContextDrawPDFPage(context, aPage);
    CGContextRestoreGState(context);    
}

- (NSArray *)_pagesInRect:(CGRect)rect
{
    NSMutableArray *pages = [NSMutableArray array];
    size_t pageCount = CGPDFDocumentGetNumberOfPages(_PDFDocument);
    CGRect *firstFoundRect = (CGRect *)bsearch(&rect, _pageRects, pageCount, sizeof(CGRect), comparePageRects);
    if (firstFoundRect != NULL) {
        size_t firstFoundIndex = firstFoundRect - _pageRects;
        id page = (id)CGPDFDocumentGetPage(_PDFDocument, firstFoundIndex+1);
        ASSERT(page);
        if (!page)
            return pages;
        [pages addObject:page];
        size_t i;
        for (i = firstFoundIndex - 1; i < pageCount; i--) {
            if (CGRectIntersectsRect(CGRectInset(_pageRects[i], 0.0, - PAGE_HEIGHT_INSET * 2), rect)) {
                id page = (id)CGPDFDocumentGetPage(_PDFDocument, i + 1);
                ASSERT(page);
                if (page)
                    [pages addObject:page];
            } else
                break;
        }
        for (i = firstFoundIndex + 1; i < pageCount; i++) {
            if (CGRectIntersectsRect(CGRectInset(_pageRects[i], 0.0, - PAGE_HEIGHT_INSET * 2), rect)) {
                id page = (id)CGPDFDocumentGetPage(_PDFDocument, i + 1);
                ASSERT(page);
                if (page)
                    [pages addObject:page];
            } else
                break;
        }        
    }
    return pages;
}

- (void)drawRect:(CGRect)aRect
{
    CGContextRef context = WKGetCurrentGraphicsContext();

    // Draw Background.
    CGContextSaveGState(context);
    CGContextSetFillColorWithColor(context, [[self class] backgroundColor]);
    CGContextFillRect(context, aRect);
    CGContextRestoreGState(context);

    if (!_PDFDocument)
        return;
    
    CGPDFPageRef page = NULL;
    NSEnumerator * enumerator = [[self _pagesInRect:aRect] objectEnumerator];

    while ((page = (CGPDFPageRef)[enumerator nextObject]))
        [self drawPage:page];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    // Since this class is a WebDocumentView and WebDocumentRepresentation, setDataSource: will be called twice. Do work only once.
    if (dataSourceHasBeenSet)
        return;

    if (!_title)
        _title = adoptNS([[[[dataSource request] URL] lastPathComponent] copy]);

    WAKView * superview = [self superview];
    
    // As noted above, -setDataSource: will be called twice -- once for the WebDocumentRepresentation, once for the WebDocumentView.
    if (!superview)
        return;

    [self setBoundsSize:[self convertRect:[superview bounds] fromView:superview].size];
    
    dataSourceHasBeenSet = YES;
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{

}

- (void)setNeedsLayout:(BOOL)flag
{

}

- (void)layout
{
    // <rdar://problem/7790957> Problem with UISplitViewController on iPad when displaying PDF
    // Since we don't have anything to layout, just repaint the visible tiles.
    [self setNeedsDisplay:YES];
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{

}

- (void)viewDidMoveToHostWindow
{

}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
}

- (void)_computePageRects
{
    size_t pageCount = CGPDFDocumentGetNumberOfPages(_PDFDocument);
    _pageRects = (CGRect *)malloc(sizeof(CGRect) * pageCount);    
    
    CGSize size = CGSizeMake(0.0, PAGE_HEIGHT_INSET);
    size_t i;
    for (i = 1; i <= pageCount; i++) {
        
        CGPDFPageRef page = CGPDFDocumentGetPage(_PDFDocument, i);
        CGRect boxRect = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
        CGFloat rotation = CGPDFPageGetRotationAngle(page) * (M_PI / 180);
        if (rotation != 0) {
            boxRect = CGRectApplyAffineTransform(boxRect, CGAffineTransformMakeRotation(rotation));
            boxRect.size.width = roundf(boxRect.size.width);
            boxRect.size.height = roundf(boxRect.size.height);
        }

        _pageRects[i-1] = boxRect;
        _pageRects[i-1].origin.y = size.height;

        size.height += boxRect.size.height + PAGE_HEIGHT_INSET;
        size.width = max(size.width, boxRect.size.width);
    }
    
    size.width += PAGE_WIDTH_INSET * 2.0;
    [self setBoundsSize:size];
    
    for (i = 0; i < pageCount; i++)
        _pageRects[i].origin.x = roundf((size.width - _pageRects[i].size.width) / 2);
}

- (void)_checkPDFTitle
{
    if (!_PDFDocument)
        return;

    RetainPtr<CFStringRef> title;

    CGPDFDictionaryRef info = CGPDFDocumentGetInfo(_PDFDocument);
    CGPDFStringRef value;
    if (CGPDFDictionaryGetString(info, "Title", &value))
        title = adoptCF(CGPDFStringCopyTextString(value));

    if (title && CFStringGetLength(title.get())) {
        _title = (NSString *)title.get();
        core([self _frame])->loader().client().dispatchDidReceiveTitle({ _title.get(), TextDirection::LTR });
    }
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    auto provider = adoptCF(CGDataProviderCreateWithCFData((CFDataRef)[dataSource data]));
    if (!provider) 
        return;
    
    _PDFDocument = CGPDFDocumentCreateWithProvider(provider.get());
    if (!_PDFDocument)
        return;
    
    [self _checkPDFTitle];
    [self _computePageRects];

    NSArray *scripts = allScriptsInPDFDocument(_PDFDocument);

    if (![scripts count])
        return;

    JSGlobalContextRef ctx = JSGlobalContextCreate(0);
    JSObjectRef jsPDFDoc = makeJSPDFDoc(ctx, dataSource);
    for (NSString *script in scripts)
        JSEvaluateScript(ctx, OpaqueJSString::tryCreate(script).get(), jsPDFDoc, nullptr, 0, nullptr);
    JSGlobalContextRelease(ctx);

    [self setNeedsDisplay:YES];
}

- (BOOL)canProvideDocumentSource
{
    return NO;
}

- (NSString *)documentSource
{
    return nil;
}

- (NSString *)title
{
    return _title.get();
}

- (unsigned)pageNumberForRect:(CGRect)rect
{
    size_t bestPageNumber = 0;
    if (_PDFDocument != NULL && _pageRects != NULL) {
        NSArray *pages = [self _pagesInRect:rect];
        float bestPageArea = 0;
        size_t count = [pages count];
        size_t i;
        for (i = 0; i < count; i++) {
            size_t pageNumber = CGPDFPageGetPageNumber((CGPDFPageRef)[pages objectAtIndex:i]);
            CGRect intersectionRect = CGRectIntersection(_pageRects[pageNumber - 1], rect);
            float intersectionArea = intersectionRect.size.width * intersectionRect.size.height;
            if (intersectionArea > bestPageArea) {
                bestPageArea = intersectionArea;
                bestPageNumber = pageNumber;
            }
        }
    }
    return bestPageNumber;
}

- (unsigned)totalPages
{
    if (_PDFDocument != NULL)
        return CGPDFDocumentGetNumberOfPages(_PDFDocument);
    return 0;
}

- (CGPDFDocumentRef)doc
{
    return _PDFDocument;
}

- (CGRect)rectForPageNumber:(unsigned)pageNum
{
    return (_pageRects != NULL && pageNum > 0 ? _pageRects[pageNum - 1] : CGRectNull);
}

@end

static int comparePageRects(const void *key, const void *array)
{
    const CGRect *keyRect = (const CGRect *)key;
    const CGRect *arrayRect = (const CGRect *)array;
    if (CGRectIntersectsRect(*arrayRect, *keyRect))
        return 0;
    return CGRectGetMinY(*keyRect) > CGRectGetMaxY(*arrayRect) ? 1 : -1;
}

#endif // PLATFORM(IOS_FAMILY)
