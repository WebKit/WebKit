/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKPrintingView.h"

#import "Logging.h"
#import "PrintInfo.h"
#import "WebData.h"
#import "WebPageProxy.h"

using namespace WebKit;
using namespace WebCore;

NSString * const WebKitOriginalTopPrintingMarginKey = @"WebKitOriginalTopMargin";
NSString * const WebKitOriginalBottomPrintingMarginKey = @"WebKitOriginalBottomMargin";

NSString * const NSPrintInfoDidChangeNotification = @"NSPrintInfoDidChange";

static BOOL isForcingPreviewUpdate;

@implementation WKPrintingView

- (id)initWithFrameProxy:(WebKit::WebFrameProxy*)frame view:(NSView *)wkView
{
    self = [super init]; // No frame rect to pass to NSView.
    if (!self)
        return nil;

    _webFrame = frame;
    _wkView = wkView;

    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)_setAutodisplay:(BOOL)newState
{
    if (!newState && [[_wkView.get() window] isAutodisplay])
        [_wkView.get() displayIfNeeded];
    
    [[_wkView.get() window] setAutodisplay:newState];

    // For some reason, painting doesn't happen for a long time without this call, <rdar://problem/8975229>.
    if (newState)
        [_wkView.get() displayIfNeeded];
}


- (void)_suspendAutodisplay
{
    // A drawRect: call on WKView causes a switch to screen mode, which is slow due to relayout, and we want to avoid that.
    // Disabling autodisplay will prevent random updates from causing this, but resizing the window will still work.
    if (_autodisplayResumeTimer) {
        [_autodisplayResumeTimer invalidate];
        _autodisplayResumeTimer = nil;
    } else
        [self _setAutodisplay:NO];
}

- (void)_delayedResumeAutodisplayTimerFired
{
    ASSERT(isMainThread());
    
    _autodisplayResumeTimer = nil;
    [self _setAutodisplay:YES];
}

- (void)_delayedResumeAutodisplay
{
    // AppKit calls endDocument/beginDocument when print option change. We don't want to switch between print and screen mode just for that,
    // and enabling autodisplay may result in switching into screen mode. So, autodisplay is only resumed on next run loop iteration.
    if (!_autodisplayResumeTimer) {
        _autodisplayResumeTimer = [NSTimer timerWithTimeInterval:0 target:self selector:@selector(_delayedResumeAutodisplayTimerFired) userInfo:nil repeats:NO];
        // The timer must be scheduled on main thread, because printing thread may finish before it fires.
        [[NSRunLoop mainRunLoop] addTimer:_autodisplayResumeTimer forMode:NSDefaultRunLoopMode];
    }
}

- (void)_adjustPrintingMarginsForHeaderAndFooter
{
    NSPrintInfo *info = [_printOperation printInfo];
    NSMutableDictionary *infoDictionary = [info dictionary];

    // We need to modify the top and bottom margins in the NSPrintInfo to account for the space needed by the
    // header and footer. Because this method can be called more than once on the same NSPrintInfo (see 5038087),
    // we stash away the unmodified top and bottom margins the first time this method is called, and we read from
    // those stashed-away values on subsequent calls.
    double originalTopMargin;
    double originalBottomMargin;
    NSNumber *originalTopMarginNumber = [infoDictionary objectForKey:WebKitOriginalTopPrintingMarginKey];
    if (!originalTopMarginNumber) {
        ASSERT(![infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey]);
        originalTopMargin = [info topMargin];
        originalBottomMargin = [info bottomMargin];
        [infoDictionary setObject:[NSNumber numberWithDouble:originalTopMargin] forKey:WebKitOriginalTopPrintingMarginKey];
        [infoDictionary setObject:[NSNumber numberWithDouble:originalBottomMargin] forKey:WebKitOriginalBottomPrintingMarginKey];
    } else {
        ASSERT([originalTopMarginNumber isKindOfClass:[NSNumber class]]);
        ASSERT([[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] isKindOfClass:[NSNumber class]]);
        originalTopMargin = [originalTopMarginNumber doubleValue];
        originalBottomMargin = [[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] doubleValue];
    }
    
    CGFloat scale = [info scalingFactor];
    [info setTopMargin:originalTopMargin + _webFrame->page()->headerHeight(_webFrame.get()) * scale];
    [info setBottomMargin:originalBottomMargin + _webFrame->page()->footerHeight(_webFrame.get()) * scale];
}

- (BOOL)_isPrintingPreview
{
    // <rdar://problem/8901041> Please add an API returning whether the current print operation is for preview.
    // Assuming that if NSPrintOperation is allowed to spawn a thread for printing, it will. Print preview doesn't spawn a thread.
    return !_isPrintingFromSecondaryThread;
}

- (void)_updatePreview
{
    // <rdar://problem/8900923> Please add an API to force print preview update.
    ASSERT(!isForcingPreviewUpdate);
    isForcingPreviewUpdate = YES;
    [[NSNotificationCenter defaultCenter] postNotificationName:NSPrintInfoDidChangeNotification object:nil];
    isForcingPreviewUpdate = NO;
}

- (BOOL)_hasPageRects
{
    // WebCore always prints at least one page.
    return !_printingPageRects.isEmpty();
}

- (NSUInteger)_firstPrintedPageNumber
{
    // Need to directly access the dictionary because -[NSPrintOperation pageRange] verifies pagination, potentially causing recursion.
    return [[[[_printOperation printInfo] dictionary] objectForKey:NSPrintFirstPage] unsignedIntegerValue];
}

- (NSUInteger)_lastPrintedPageNumber
{
    ASSERT([self _hasPageRects]);

    // Need to directly access the dictionary because -[NSPrintOperation pageRange] verifies pagination, potentially causing recursion.
    NSUInteger firstPage = [[[[_printOperation printInfo] dictionary] objectForKey:NSPrintFirstPage] unsignedIntegerValue];
    NSUInteger lastPage = [[[[_printOperation printInfo] dictionary] objectForKey:NSPrintLastPage] unsignedIntegerValue];
    if (lastPage - firstPage >= _printingPageRects.size())
        return _printingPageRects.size();
    return lastPage;
}

- (uint64_t)_expectedPreviewCallbackForRect:(const IntRect&)rect
{
    for (HashMap<uint64_t, WebCore::IntRect>::iterator iter = _expectedPreviewCallbacks.begin(); iter != _expectedPreviewCallbacks.end(); ++iter) {
        if (iter->second  == rect)
            return iter->first;
    }
    return 0;
}

struct IPCCallbackContext {
    RetainPtr<WKPrintingView> view;
    uint64_t callbackID;
};

static void pageDidDrawToPDF(WKDataRef dataRef, WKErrorRef, void* untypedContext)
{
    ASSERT(isMainThread());

    OwnPtr<IPCCallbackContext> context = adoptPtr(static_cast<IPCCallbackContext*>(untypedContext));
    WKPrintingView *view = context->view.get();
    WebData* data = toImpl(dataRef);

    if (context->callbackID == view->_expectedPrintCallback) {
        ASSERT(![view _isPrintingPreview]);
        ASSERT(view->_printedPagesData.isEmpty());
        ASSERT(!view->_printedPagesPDFDocument);
        if (data)
            view->_printedPagesData.append(data->bytes(), data->size());
        view->_expectedPrintCallback = 0;
        view->_printingCallbackCondition.signal();
    } else {
        // If the user has already changed print setup, then this response is obsolete. And this callback is not in response to the latest request,
        // then the user has already moved to another page - we'll cache the response, but won't draw it.
        HashMap<uint64_t, WebCore::IntRect>::iterator iter = view->_expectedPreviewCallbacks.find(context->callbackID);
        if (iter != view->_expectedPreviewCallbacks.end()) {
            ASSERT([view _isPrintingPreview]);

            if (data) {
                pair<HashMap<WebCore::IntRect, Vector<uint8_t> >::iterator, bool> entry = view->_pagePreviews.add(iter->second, Vector<uint8_t>());
                entry.first->second.append(data->bytes(), data->size());
            }
            view->_expectedPreviewCallbacks.remove(context->callbackID);
            bool receivedResponseToLatestRequest = view->_latestExpectedPreviewCallback == context->callbackID;
            if (receivedResponseToLatestRequest) {
                view->_latestExpectedPreviewCallback = 0;
                [view _updatePreview];
            }
        }
    }
}

- (void)_preparePDFDataForPrintingOnSecondaryThread
{
    ASSERT(isMainThread());

    if (!_webFrame->page()) {
        _printingCallbackCondition.signal();
        return;
    }

    MutexLocker lock(_printingCallbackMutex);

    ASSERT([self _hasPageRects]);
    ASSERT(_printedPagesData.isEmpty());
    ASSERT(!_printedPagesPDFDocument);
    ASSERT(!_expectedPrintCallback);

    NSUInteger firstPage = [self _firstPrintedPageNumber];
    NSUInteger lastPage = [self _lastPrintedPageNumber];

    ASSERT(firstPage > 0);
    ASSERT(firstPage <= lastPage);
    LOG(View, "WKPrintingView requesting PDF data for pages %u...%u", firstPage, lastPage);

    // Return to printing mode if we're already back to screen (e.g. due to window resizing).
    _webFrame->page()->beginPrinting(_webFrame.get(), PrintInfo([_printOperation printInfo]));

    IPCCallbackContext* context = new IPCCallbackContext;
    RefPtr<DataCallback> callback = DataCallback::create(context, pageDidDrawToPDF);
    _expectedPrintCallback = callback->callbackID();

    context->view = self;
    context->callbackID = callback->callbackID();

    _webFrame->page()->drawPagesToPDF(_webFrame.get(), firstPage - 1, lastPage - firstPage + 1, callback.get());
}

static void pageDidComputePageRects(const Vector<WebCore::IntRect>& pageRects, double totalScaleFactorForPrinting, WKErrorRef, void* untypedContext)
{
    ASSERT(isMainThread());

    OwnPtr<IPCCallbackContext> context = adoptPtr(static_cast<IPCCallbackContext*>(untypedContext));
    WKPrintingView *view = context->view.get();

    // If the user has already changed print setup, then this response is obsolete.
    if (context->callbackID == view->_expectedComputedPagesCallback) {
        ASSERT(isMainThread());
        ASSERT(view->_expectedPreviewCallbacks.isEmpty());
        ASSERT(!view->_latestExpectedPreviewCallback);
        ASSERT(!view->_expectedPrintCallback);
        ASSERT(view->_pagePreviews.isEmpty());
        view->_expectedComputedPagesCallback = 0;

        view->_printingPageRects = pageRects;
        view->_totalScaleFactorForPrinting = totalScaleFactorForPrinting;

        // Sanitize a response coming from the Web process.
        if (view->_printingPageRects.isEmpty())
            view->_printingPageRects.append(IntRect(0, 0, 1, 1));
        if (view->_totalScaleFactorForPrinting <= 0)
            view->_totalScaleFactorForPrinting = 1;

        const IntRect& lastPrintingPageRect = view->_printingPageRects[view->_printingPageRects.size() - 1];
        NSRect newFrameSize = NSMakeRect(0, 0, 
            ceil(lastPrintingPageRect.maxX() * view->_totalScaleFactorForPrinting), 
            ceil(lastPrintingPageRect.maxY() * view->_totalScaleFactorForPrinting));
        LOG(View, "WKPrintingView setting frame size to x:%g y:%g width:%g height:%g", newFrameSize.origin.x, newFrameSize.origin.y, newFrameSize.size.width, newFrameSize.size.height);
        [view setFrame:newFrameSize];

        if ([view _isPrintingPreview]) {
            // Show page count, and ask for an actual image to replace placeholder.
            [view _updatePreview];
        } else {
            // When printing, request everything we'll need beforehand.
            [view _preparePDFDataForPrintingOnSecondaryThread];
        }
    }
}

- (BOOL)_askPageToComputePageRects
{
    ASSERT(isMainThread());

    if (!_webFrame->page())
        return NO;

    ASSERT(!_expectedComputedPagesCallback);

    IPCCallbackContext* context = new IPCCallbackContext;
    RefPtr<ComputedPagesCallback> callback = ComputedPagesCallback::create(context, pageDidComputePageRects);
    _expectedComputedPagesCallback = callback->callbackID();
    context->view = self;
    context->callbackID = _expectedComputedPagesCallback;

    _webFrame->page()->computePagesForPrinting(_webFrame.get(), PrintInfo([_printOperation printInfo]), callback.release());
    return YES;
}

static void prepareDataForPrintingOnSecondaryThread(void* untypedContext)
{
    ASSERT(isMainThread());

    WKPrintingView *view = static_cast<WKPrintingView *>(untypedContext);
    MutexLocker lock(view->_printingCallbackMutex);

    // We may have received page rects while a message to call this function traveled from secondary thread to main one.
    if ([view _hasPageRects]) {
        [view _preparePDFDataForPrintingOnSecondaryThread];
        return;
    }

    // A request for pages has already been made, just wait for it to finish.
    if (view->_expectedComputedPagesCallback)
        return;

    [view _askPageToComputePageRects];
}

- (BOOL)knowsPageRange:(NSRangePointer)range
{
    LOG(View, "-[WKPrintingView %p knowsPageRange:], %s, %s", self, [self _hasPageRects] ? "print data is available" : "print data is not available yet", isMainThread() ? "on main thread" : "on secondary thread");
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    // Assuming that once we switch to printing from a secondary thread, we don't go back.
    ASSERT(!_isPrintingFromSecondaryThread || !isMainThread());
    if (!isMainThread())
        _isPrintingFromSecondaryThread = YES;

    if (!_webFrame->page()) {
        *range = NSMakeRange(1, NSIntegerMax);
        return YES;
    }

    [self _suspendAutodisplay];
    
    [self _adjustPrintingMarginsForHeaderAndFooter];

    if ([self _hasPageRects])
        *range = NSMakeRange(1, _printingPageRects.size());
    else if (!isMainThread()) {
        ASSERT(![self _isPrintingPreview]);
        MutexLocker lock(_printingCallbackMutex);
        callOnMainThread(prepareDataForPrintingOnSecondaryThread, self);
        _printingCallbackCondition.wait(_printingCallbackMutex);
        *range = NSMakeRange(1, _printingPageRects.size());
    } else {
        ASSERT([self _isPrintingPreview]);

        // If a request for pages hasn't already been made, make it now.
        if (!_expectedComputedPagesCallback)
            [self _askPageToComputePageRects];

        *range = NSMakeRange(1, NSIntegerMax);
    }
    return YES;
}

- (unsigned)_pageForRect:(NSRect)rect
{
    // Assuming that rect exactly matches one of the pages.
    for (size_t i = 0; i < _printingPageRects.size(); ++i) {
        IntRect currentRect(_printingPageRects[i]);
        currentRect.scale(_totalScaleFactorForPrinting);
        if (rect.origin.y == currentRect.y() && rect.origin.x == currentRect.x())
            return i + 1;
    }
    ASSERT_NOT_REACHED();
    return 0; // Invalid page number.
}

- (void)_drawPDFDocument:(CGPDFDocumentRef)pdfDocument page:(unsigned)page atPoint:(NSPoint)point
{
    if (!pdfDocument) {
        LOG_ERROR("Couldn't create a PDF document with data passed for preview");
        return;
    }

    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfDocument, page);
    if (!pdfPage) {
        LOG_ERROR("Preview data doesn't have page %d", page);
        return;
    }

    NSGraphicsContext *nsGraphicsContext = [NSGraphicsContext currentContext];
    CGContextRef context = static_cast<CGContextRef>([nsGraphicsContext graphicsPort]);

    CGContextSaveGState(context);
    CGContextTranslateCTM(context, point.x, point.y);
    CGContextScaleCTM(context, _totalScaleFactorForPrinting, -_totalScaleFactorForPrinting);
    CGContextTranslateCTM(context, 0, -CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox).size.height);
    CGContextDrawPDFPage(context, pdfPage);
    CGContextRestoreGState(context);
}

- (void)_drawPreview:(NSRect)nsRect
{
    ASSERT(isMainThread());

    IntRect rect(nsRect);
    rect.scale(1 / _totalScaleFactorForPrinting);
    HashMap<WebCore::IntRect, Vector<uint8_t> >::iterator pagePreviewIterator = _pagePreviews.find(rect);
    if (pagePreviewIterator == _pagePreviews.end())  {
        // It's too early to ask for page preview if we don't even know page size and scale.
        if ([self _hasPageRects]) {
            if (uint64_t existingCallback = [self _expectedPreviewCallbackForRect:rect]) {
                // We've already asked for a preview of this page, and are waiting for response.
                // There is no need to ask again.
                _latestExpectedPreviewCallback = existingCallback;
            } else {
                // Preview isn't available yet, request it asynchronously.
                if (!_webFrame->page())
                    return;

                // Return to printing mode if we're already back to screen (e.g. due to window resizing).
                _webFrame->page()->beginPrinting(_webFrame.get(), PrintInfo([_printOperation printInfo]));

                IPCCallbackContext* context = new IPCCallbackContext;
                RefPtr<DataCallback> callback = DataCallback::create(context, pageDidDrawToPDF);
                _latestExpectedPreviewCallback = callback->callbackID();
                _expectedPreviewCallbacks.add(_latestExpectedPreviewCallback, rect);

                context->view = self;
                context->callbackID = callback->callbackID();

                _webFrame->page()->drawRectToPDF(_webFrame.get(), rect, callback.get());
                return;
            }
        }

        // FIXME: Draw a placeholder
        return;
    }

    const Vector<uint8_t>& pdfData = pagePreviewIterator->second;
    RetainPtr<CGDataProviderRef> pdfDataProvider(AdoptCF, CGDataProviderCreateWithData(0, pdfData.data(), pdfData.size(), 0));
    RetainPtr<CGPDFDocumentRef> pdfDocument(AdoptCF, CGPDFDocumentCreateWithProvider(pdfDataProvider.get()));

    [self _drawPDFDocument:pdfDocument.get() page:1 atPoint:NSMakePoint(nsRect.origin.x, nsRect.origin.y)];
}

- (void)drawRect:(NSRect)nsRect
{
    LOG(View, "WKPrintingView %p printing rect x:%g, y:%g, width:%g, height:%g%s", self, nsRect.origin.x, nsRect.origin.y, nsRect.size.width, nsRect.size.height, [self _isPrintingPreview] ? " for preview" : "");

    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    if (!_webFrame->page())
        return;

    if ([self _isPrintingPreview]) {
        [self _drawPreview:nsRect];
        return;
    }

    ASSERT(!isMainThread());
    ASSERT(!_printedPagesData.isEmpty()); // Prepared by knowsPageRange:

    if (!_printedPagesPDFDocument) {
        RetainPtr<CGDataProviderRef> pdfDataProvider(AdoptCF, CGDataProviderCreateWithData(0, _printedPagesData.data(), _printedPagesData.size(), 0));
        _printedPagesPDFDocument.adoptCF(CGPDFDocumentCreateWithProvider(pdfDataProvider.get()));
    }

    unsigned printedPageNumber = [self _pageForRect:nsRect] - [self _firstPrintedPageNumber] + 1;
    [self _drawPDFDocument:_printedPagesPDFDocument.get() page:printedPageNumber atPoint:NSMakePoint(nsRect.origin.x, nsRect.origin.y)];
}

- (void)_drawPageBorderWithSizeOnMainThread:(NSSize)borderSize
{
    ASSERT(isMainThread());

    // When printing from a secondary thread, the main thread doesn't have graphics context and printing operation set up.
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:[_printOperation context]];

    ASSERT(![NSPrintOperation currentOperation]);
    [NSPrintOperation setCurrentOperation:_printOperation];

    [self drawPageBorderWithSize:borderSize];

    [NSPrintOperation setCurrentOperation:nil];
    [NSGraphicsContext setCurrentContext:currentContext];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[_printOperation printInfo] paperSize]));    
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    if (!isMainThread()) {
        // Don't call the client from a secondary thread.
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[WKPrintingView instanceMethodSignatureForSelector:@selector(_drawPageBorderWithSizeOnMainThread:)]];
        [invocation setSelector:@selector(_drawPageBorderWithSizeOnMainThread:)];
        [invocation setArgument:&borderSize atIndex:2];
        [invocation performSelectorOnMainThread:@selector(invokeWithTarget:) withObject:self waitUntilDone:YES];
        return;
    }

    if (!_webFrame->page())
        return;

    // The header and footer rect height scales with the page, but the width is always
    // all the way across the printed page (inset by printing margins).
    NSPrintInfo *printInfo = [_printOperation printInfo];
    CGFloat scale = [printInfo scalingFactor];
    NSSize paperSize = [printInfo paperSize];
    CGFloat headerFooterLeft = [printInfo leftMargin] / scale;
    CGFloat headerFooterWidth = (paperSize.width - ([printInfo leftMargin] + [printInfo rightMargin])) / scale;
    NSRect footerRect = NSMakeRect(headerFooterLeft, [printInfo bottomMargin] / scale - _webFrame->page()->footerHeight(_webFrame.get()), headerFooterWidth, _webFrame->page()->footerHeight(_webFrame.get()));
    NSRect headerRect = NSMakeRect(headerFooterLeft, (paperSize.height - [printInfo topMargin]) / scale, headerFooterWidth, _webFrame->page()->headerHeight(_webFrame.get()));

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    NSRectClip(headerRect);
    _webFrame->page()->drawHeader(_webFrame.get(), headerRect);
    [currentContext restoreGraphicsState];

    [currentContext saveGraphicsState];
    NSRectClip(footerRect);
    _webFrame->page()->drawFooter(_webFrame.get(), footerRect);
    [currentContext restoreGraphicsState];
}

- (NSRect)rectForPage:(NSInteger)page
{
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);
    if (![self _hasPageRects]) {
        LOG(View, "-[WKPrintingView %p rectForPage:%d] - data is not yet available", self, (int)page);
        if (!_webFrame->page()) {
            // We may have not told AppKit how many pages there are, so it will try to print until a null rect is returned.
            return NSMakeRect(0, 0, 0, 0);
        }
        // We must be still calculating the page range.
        ASSERT(_expectedComputedPagesCallback);
        return NSMakeRect(0, 0, 1, 1);
    }

    // If Web process crashes while computing page rects, we never tell AppKit how many pages there are.
    // Returning a null rect prevents selecting non-existent pages in preview dialog.
    if (static_cast<unsigned>(page) > _printingPageRects.size()) {
        ASSERT(!_webFrame->page());
        return NSMakeRect(0, 0, 0, 0);
    }

    IntRect rect = _printingPageRects[page - 1];
    rect.scale(_totalScaleFactorForPrinting);
    LOG(View, "-[WKPrintingView %p rectForPage:%d] -> x %d, y %d, width %d, height %d", self, (int)page, rect.x(), rect.y(), rect.width(), rect.height());
    return rect;
}

// Temporary workaround for <rdar://problem/8944535>. Force correct printout positioning.
- (NSPoint)locationOfPrintRect:(NSRect)aRect
{
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);
    return NSMakePoint([[_printOperation printInfo] leftMargin], [[_printOperation printInfo] bottomMargin]);
}

- (void)beginDocument
{
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    // Forcing preview update gets us here, but page setup hasn't actually changed.
    if (isForcingPreviewUpdate)
        return;

    LOG(View, "-[WKPrintingView %p beginDocument]", self);

    [super beginDocument];

    [self _suspendAutodisplay];
}

- (void)endDocument
{
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    // Forcing preview update gets us here, but page setup hasn't actually changed.
    if (isForcingPreviewUpdate)
        return;

    LOG(View, "-[WKPrintingView %p endDocument] - clearing cached data", self);

    // Both existing data and pending responses are now obsolete.
    _printingPageRects.clear();
    _totalScaleFactorForPrinting = 1;
    _pagePreviews.clear();
    _printedPagesData.clear();
    _printedPagesPDFDocument = nullptr;
    _expectedComputedPagesCallback = 0;
    _expectedPreviewCallbacks.clear();
    _latestExpectedPreviewCallback = 0;
    _expectedPrintCallback = 0;

    [self _delayedResumeAutodisplay];
    
    [super endDocument];
}
@end
