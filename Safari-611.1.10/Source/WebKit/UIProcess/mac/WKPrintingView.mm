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

#if PLATFORM(MAC)

#import "APIData.h"
#import "Logging.h"
#import "PDFKitImports.h"
#import "PrintInfo.h"
#import "ShareableBitmap.h"
#import "WebPageProxy.h"
#import <Quartz/Quartz.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/LocalDefaultSystemAppearance.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RunLoop.h>

NSString * const WebKitOriginalTopPrintingMarginKey = @"WebKitOriginalTopMargin";
NSString * const WebKitOriginalBottomPrintingMarginKey = @"WebKitOriginalBottomMargin";

NSString * const NSPrintInfoDidChangeNotification = @"NSPrintInfoDidChange";

static BOOL isForcingPreviewUpdate;

@implementation WKPrintingView

- (id)initWithFrameProxy:(WebKit::WebFrameProxy&)frame view:(NSView *)wkView
{
    self = [super init]; // No frame rect to pass to NSView.
    if (!self)
        return nil;

    _webFrame = &frame;
    _wkView = wkView;

    return self;
}

- (void)dealloc
{
    callOnMainThread([frame = WTFMove(_webFrame), previews = WTFMove(_pagePreviews)] {
        // Deallocate these on the main thread, not the current thread, since the
        // reference counting and the destructors aren't threadsafe.
    });

    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)_setAutodisplay:(BOOL)newState
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!newState && [[_wkView window] isAutodisplay])
        [_wkView displayIfNeeded];
    ALLOW_DEPRECATED_DECLARATIONS_END
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[_wkView window] setAutodisplay:newState];
    ALLOW_DEPRECATED_DECLARATIONS_END

    // For some reason, painting doesn't happen for a long time without this call, <rdar://problem/8975229>.
    if (newState)
        [_wkView displayIfNeeded];
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
    ASSERT(RunLoop::isMain());
    
    _autodisplayResumeTimer = nil;
    [self _setAutodisplay:YES];

    // Enabling autodisplay normally implicitly calls endPrinting() via -[WKView drawRect:], but not when content is in accelerated compositing mode.
    if (_webFrame->page())
        _webFrame->page()->endPrinting();
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
    ASSERT(RunLoop::isMain()); // This function calls the client, which should only be done on main thread.

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
        [infoDictionary setObject:@(originalTopMargin) forKey:WebKitOriginalTopPrintingMarginKey];
        [infoDictionary setObject:@(originalBottomMargin) forKey:WebKitOriginalBottomPrintingMarginKey];
    } else {
        ASSERT([originalTopMarginNumber isKindOfClass:[NSNumber class]]);
        ASSERT([[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] isKindOfClass:[NSNumber class]]);
        originalTopMargin = [originalTopMarginNumber doubleValue];
        originalBottomMargin = [[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] doubleValue];
    }
    
    CGFloat scale = [info scalingFactor];
    [info setTopMargin:originalTopMargin + _webFrame->page()->headerHeight(*_webFrame) * scale];
    [info setBottomMargin:originalBottomMargin + _webFrame->page()->footerHeight(*_webFrame) * scale];
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

- (uint64_t)_expectedPreviewCallbackForRect:(const WebCore::IntRect&)rect
{
    for (HashMap<uint64_t, WebCore::IntRect>::iterator iter = _expectedPreviewCallbacks.begin(); iter != _expectedPreviewCallbacks.end(); ++iter) {
        if (iter->value  == rect)
            return iter->key;
    }
    return 0;
}

struct IPCCallbackContext {
    RetainPtr<WKPrintingView> view;
    uint64_t callbackID;
};

static void pageDidDrawToImage(const WebKit::ShareableBitmap::Handle& imageHandle, IPCCallbackContext* context)
{
    ASSERT(RunLoop::isMain());

    WKPrintingView *view = context->view.get();

    // If the user has already changed print setup, then this response is obsolete. And if this callback is not in response to the latest request,
    // then the user has already moved to another page - we'll cache the response, but won't draw it.
    HashMap<uint64_t, WebCore::IntRect>::iterator iter = view->_expectedPreviewCallbacks.find(context->callbackID);
    if (iter != view->_expectedPreviewCallbacks.end()) {
        ASSERT([view _isPrintingPreview]);

        if (!imageHandle.isNull()) {
            auto image = WebKit::ShareableBitmap::create(imageHandle, WebKit::SharedMemory::Protection::ReadOnly);

            if (image)
                view->_pagePreviews.add(iter->value, image);
        }

        view->_expectedPreviewCallbacks.remove(context->callbackID);
        bool receivedResponseToLatestRequest = view->_latestExpectedPreviewCallback == context->callbackID;
        if (receivedResponseToLatestRequest) {
            view->_latestExpectedPreviewCallback = 0;
            [view _updatePreview];
        }
    }
}

- (void)_preparePDFDataForPrintingOnSecondaryThread
{
    ASSERT(RunLoop::isMain());

    if (!_webFrame->page()) {
        _printingCallbackCondition.notifyOne();
        return;
    }

    auto locker = holdLock(_printingCallbackMutex);

    ASSERT([self _hasPageRects]);
    ASSERT(_printedPagesData.isEmpty());
    ASSERT(!_printedPagesPDFDocument);
    ASSERT(!_expectedPrintCallback);

    NSUInteger firstPage = [self _firstPrintedPageNumber];
    NSUInteger lastPage = [self _lastPrintedPageNumber];

    ASSERT(firstPage > 0);
    ASSERT(firstPage <= lastPage);
    LOG(Printing, "WKPrintingView requesting PDF data for pages %u...%u", firstPage, lastPage);

    WebKit::PrintInfo printInfo([_printOperation printInfo]);
    // Return to printing mode if we're already back to screen (e.g. due to window resizing).
    _webFrame->page()->beginPrinting(_webFrame.get(), printInfo);

    IPCCallbackContext* context = new IPCCallbackContext;
    auto callback = WebKit::DataCallback::create([context](API::Data* data, WebKit::CallbackBase::Error) {
        ASSERT(RunLoop::isMain());

        std::unique_ptr<IPCCallbackContext> contextDeleter(context);
        WKPrintingView *view = context->view.get();

        if (context->callbackID == view->_expectedPrintCallback) {
            ASSERT(![view _isPrintingPreview]);
            ASSERT(view->_printedPagesData.isEmpty());
            ASSERT(!view->_printedPagesPDFDocument);
            if (data)
                view->_printedPagesData.append(data->bytes(), data->size());
            view->_expectedPrintCallback = 0;
            view->_printingCallbackCondition.notifyOne();
        }
    });
    _expectedPrintCallback = callback->callbackID().toInteger();

    context->view = self;
    context->callbackID = callback->callbackID().toInteger();

    _webFrame->page()->drawPagesToPDF(_webFrame.get(), printInfo, firstPage - 1, lastPage - firstPage + 1, WTFMove(callback));
}

static void pageDidComputePageRects(const Vector<WebCore::IntRect>& pageRects, double totalScaleFactorForPrinting, const WebCore::FloatBoxExtent& computedPageMargin, IPCCallbackContext* context)
{
    ASSERT(RunLoop::isMain());

    WKPrintingView *view = context->view.get();

    // If the user has already changed print setup, then this response is obsolete.
    if (context->callbackID == view->_expectedComputedPagesCallback) {
        ASSERT(RunLoop::isMain());
        ASSERT(view->_expectedPreviewCallbacks.isEmpty());
        ASSERT(!view->_latestExpectedPreviewCallback);
        ASSERT(!view->_expectedPrintCallback);
        ASSERT(view->_pagePreviews.isEmpty());
        view->_expectedComputedPagesCallback = 0;

        view->_printingPageRects = pageRects;
        view->_totalScaleFactorForPrinting = totalScaleFactorForPrinting;

        // Sanitize a response coming from the Web process.
        if (view->_printingPageRects.isEmpty())
            view->_printingPageRects.append(WebCore::IntRect(0, 0, 1, 1));
        if (view->_totalScaleFactorForPrinting <= 0)
            view->_totalScaleFactorForPrinting = 1;

        const WebCore::IntRect& lastPrintingPageRect = view->_printingPageRects[view->_printingPageRects.size() - 1];
        NSRect newFrameSize = NSMakeRect(0, 0, 
            ceil(lastPrintingPageRect.maxX() * view->_totalScaleFactorForPrinting), 
            ceil(lastPrintingPageRect.maxY() * view->_totalScaleFactorForPrinting));
        LOG(Printing, "WKPrintingView setting frame size to x:%g y:%g width:%g height:%g", newFrameSize.origin.x, newFrameSize.origin.y, newFrameSize.size.width, newFrameSize.size.height);
        [view setFrame:newFrameSize];
        // Set @page margin.
        auto *printInfo = [view->_printOperation printInfo];
        [printInfo setTopMargin:computedPageMargin.top()];
        [printInfo setBottomMargin:computedPageMargin.bottom()];
        [printInfo setLeftMargin:computedPageMargin.left()];
        [printInfo setRightMargin:computedPageMargin.right()];

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
    ASSERT(RunLoop::isMain());

    if (!_webFrame->page())
        return NO;

    ASSERT(!_expectedComputedPagesCallback);

    IPCCallbackContext* context = new IPCCallbackContext;
    auto callback = WebKit::ComputedPagesCallback::create([context](const Vector<WebCore::IntRect>& pageRects, double totalScaleFactorForPrinting, const WebCore::FloatBoxExtent& computedPageMargin, WebKit::CallbackBase::Error) {
        std::unique_ptr<IPCCallbackContext> contextDeleter(context);
        pageDidComputePageRects(pageRects, totalScaleFactorForPrinting, computedPageMargin, context);
    });
    _expectedComputedPagesCallback = callback->callbackID().toInteger();
    context->view = self;
    context->callbackID = _expectedComputedPagesCallback;

    _webFrame->page()->computePagesForPrinting(_webFrame.get(), WebKit::PrintInfo([_printOperation printInfo]), WTFMove(callback));
    return YES;
}

static void prepareDataForPrintingOnSecondaryThread(WKPrintingView *view)
{
    ASSERT(RunLoop::isMain());

    auto locker = holdLock(view->_printingCallbackMutex);

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
    LOG(Printing, "-[WKPrintingView %p knowsPageRange:], %s, %s", self, [self _hasPageRects] ? "print data is available" : "print data is not available yet", RunLoop::isMain() ? "on main thread" : "on secondary thread");
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    // Assuming that once we switch to printing from a secondary thread, we don't go back.
    ASSERT(!_isPrintingFromSecondaryThread || !RunLoop::isMain());
    if (!RunLoop::isMain())
        _isPrintingFromSecondaryThread = YES;

    if (_webFrame->pageIsClosed()) {
        *range = NSMakeRange(1, NSIntegerMax);
        return YES;
    }

    [self _suspendAutodisplay];
    
    [self performSelectorOnMainThread:@selector(_adjustPrintingMarginsForHeaderAndFooter) withObject:nil waitUntilDone:YES];

    if ([self _hasPageRects])
        *range = NSMakeRange(1, _printingPageRects.size());
    else if (!RunLoop::isMain()) {
        ASSERT(![self _isPrintingPreview]);
        std::unique_lock<Lock> lock(_printingCallbackMutex);

        RunLoop::main().dispatch([self] {
            prepareDataForPrintingOnSecondaryThread(self);
        });

        _printingCallbackCondition.wait(lock);
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
        WebCore::IntRect currentRect(_printingPageRects[i]);
        currentRect.scale(_totalScaleFactorForPrinting);
        if (rect.origin.y == currentRect.y() && rect.origin.x == currentRect.x())
            return i + 1;
    }
    ASSERT_NOT_REACHED();
    return 0; // Invalid page number.
}

static NSString *linkDestinationName(PDFDocument *document, PDFDestination *destination)
{
    return [NSString stringWithFormat:@"%lu-%f-%f", (unsigned long)[document indexForPage:destination.page], destination.point.x, destination.point.y];
}

- (void)_drawPDFDocument:(PDFDocument *)pdfDocument page:(unsigned)page atPoint:(NSPoint)point
{
    if (!pdfDocument) {
        LOG_ERROR("Couldn't create a PDF document with data passed for preview");
        return;
    }

    PDFPage *pdfPage;
    @try {
        pdfPage = [pdfDocument pageAtIndex:page];
    } @catch (id exception) {
        LOG_ERROR("Preview data doesn't have page %d: %@", page, exception);
        return;
    }

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    CGContextSaveGState(context);
    CGContextTranslateCTM(context, point.x, point.y);
    CGContextScaleCTM(context, _totalScaleFactorForPrinting, -_totalScaleFactorForPrinting);
    CGContextTranslateCTM(context, 0, -[pdfPage boundsForBox:kPDFDisplayBoxMediaBox].size.height);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [pdfPage drawWithBox:kPDFDisplayBoxMediaBox];
    ALLOW_DEPRECATED_DECLARATIONS_END

    CGAffineTransform transform = CGContextGetCTM(context);

    for (const auto& destination : _linkDestinationsPerPage[page]) {
        CGPoint destinationPoint = CGPointApplyAffineTransform(NSPointToCGPoint([destination point]), transform);
        CGPDFContextAddDestinationAtPoint(context, (__bridge CFStringRef)linkDestinationName(pdfDocument, destination.get()), destinationPoint);
    }

    for (PDFAnnotation *annotation in [pdfPage annotations]) {
        if (![annotation isKindOfClass:WebKit::pdfAnnotationLinkClass()])
            continue;

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        PDFAnnotationLink *linkAnnotation = (PDFAnnotationLink *)annotation;
        ALLOW_DEPRECATED_DECLARATIONS_END
        NSURL *url = [linkAnnotation URL];
        CGRect transformedRect = CGRectApplyAffineTransform(NSRectToCGRect([linkAnnotation bounds]), transform);

        if (!url) {
            PDFDestination *destination = [linkAnnotation destination];
            if (!destination)
                continue;
            CGPDFContextSetDestinationForRect(context, (__bridge CFStringRef)linkDestinationName(pdfDocument, destination), transformedRect);
            continue;
        }

        CGPDFContextSetURLForRect(context, (CFURLRef)url, transformedRect);
    }

    CGContextRestoreGState(context);
}

- (void)_drawPreview:(NSRect)nsRect
{
    ASSERT(RunLoop::isMain());
    
    if (!_webFrame || !_webFrame->page())
        return;

    WebCore::IntRect scaledPrintingRect(nsRect);
    scaledPrintingRect.scale(1 / _totalScaleFactorForPrinting);
    WebCore::IntSize imageSize(nsRect.size);
    imageSize.scale(_webFrame->page()->deviceScaleFactor());
    HashMap<WebCore::IntRect, RefPtr<WebKit::ShareableBitmap>>::iterator pagePreviewIterator = _pagePreviews.find(scaledPrintingRect);
    if (pagePreviewIterator == _pagePreviews.end())  {
        // It's too early to ask for page preview if we don't even know page size and scale.
        if ([self _hasPageRects]) {
            if (uint64_t existingCallback = [self _expectedPreviewCallbackForRect:scaledPrintingRect]) {
                // We've already asked for a preview of this page, and are waiting for response.
                // There is no need to ask again.
                _latestExpectedPreviewCallback = existingCallback;
            } else {
                // Preview isn't available yet, request it asynchronously.
                // Return to printing mode if we're already back to screen (e.g. due to window resizing).
                _webFrame->page()->beginPrinting(_webFrame.get(), WebKit::PrintInfo([_printOperation printInfo]));

                IPCCallbackContext* context = new IPCCallbackContext;
                auto callback = WebKit::ImageCallback::create([context](const WebKit::ShareableBitmap::Handle& imageHandle, WebKit::CallbackBase::Error) {
                    std::unique_ptr<IPCCallbackContext> contextDeleter(context);
                    pageDidDrawToImage(imageHandle, context);
                });
                _latestExpectedPreviewCallback = callback->callbackID().toInteger();
                _expectedPreviewCallbacks.add(_latestExpectedPreviewCallback, scaledPrintingRect);

                context->view = self;
                context->callbackID = callback->callbackID().toInteger();

                _webFrame->page()->drawRectToImage(_webFrame.get(), WebKit::PrintInfo([_printOperation printInfo]), scaledPrintingRect, imageSize, WTFMove(callback));
                return;
            }
        }

        // FIXME: Draw a placeholder
        return;
    }

    RefPtr<WebKit::ShareableBitmap> bitmap = pagePreviewIterator->value;

    WebCore::GraphicsContext context([[NSGraphicsContext currentContext] CGContext]);
    WebCore::GraphicsContextStateSaver stateSaver(context);

    bitmap->paint(context, _webFrame->page()->deviceScaleFactor(), WebCore::IntPoint(nsRect.origin), bitmap->bounds());
}

- (void)drawRect:(NSRect)nsRect
{
    LOG(Printing, "WKPrintingView %p printing rect x:%g, y:%g, width:%g, height:%g%s", self, nsRect.origin.x, nsRect.origin.y, nsRect.size.width, nsRect.size.height, [self _isPrintingPreview] ? " for preview" : "");

    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    // Always use the light appearance when printing.
    WebCore::LocalDefaultSystemAppearance localAppearance(false);

    if ([self _isPrintingPreview]) {
        [self _drawPreview:nsRect];
        return;
    }

    ASSERT(!RunLoop::isMain());
    ASSERT(!_printedPagesData.isEmpty()); // Prepared by knowsPageRange:

    if (!_printedPagesPDFDocument) {
        RetainPtr<NSData> pdfData = adoptNS([[NSData alloc] initWithBytes:_printedPagesData.data() length:_printedPagesData.size()]);
        _printedPagesPDFDocument = adoptNS([[WebKit::pdfDocumentClass() alloc] initWithData:pdfData.get()]);

        unsigned pageCount = [_printedPagesPDFDocument pageCount];
        _linkDestinationsPerPage.clear();
        _linkDestinationsPerPage.resize(pageCount);
        for (unsigned i = 0; i < pageCount; i++) {
            PDFPage *page = [_printedPagesPDFDocument pageAtIndex:i];
            for (PDFAnnotation *annotation in page.annotations) {
                if (![annotation isKindOfClass:WebKit::pdfAnnotationLinkClass()])
                    continue;

                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                PDFAnnotationLink *linkAnnotation = (PDFAnnotationLink *)annotation;
                ALLOW_DEPRECATED_DECLARATIONS_END
                if (linkAnnotation.URL)
                    continue;

                PDFDestination *destination = linkAnnotation.destination;
                if (!destination)
                    continue;

                unsigned destinationPageIndex = [_printedPagesPDFDocument indexForPage:destination.page];
                _linkDestinationsPerPage[destinationPageIndex].append(destination);
            }
        }
    }

    unsigned printedPageNumber = [self _pageForRect:nsRect] - [self _firstPrintedPageNumber];
    [self _drawPDFDocument:_printedPagesPDFDocument.get() page:printedPageNumber atPoint:NSMakePoint(nsRect.origin.x, nsRect.origin.y)];
}

- (void)_drawPageBorderWithSizeOnMainThread:(NSSize)borderSize
{
    ASSERT(RunLoop::isMain());

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

    if (!RunLoop::isMain()) {
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
    NSRect footerRect = NSMakeRect(headerFooterLeft, [printInfo bottomMargin] / scale - _webFrame->page()->footerHeight(*_webFrame), headerFooterWidth, _webFrame->page()->footerHeight(*_webFrame));
    NSRect headerRect = NSMakeRect(headerFooterLeft, (paperSize.height - [printInfo topMargin]) / scale, headerFooterWidth, _webFrame->page()->headerHeight(*_webFrame));

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    NSRectClip(headerRect);
    _webFrame->page()->drawHeader(*_webFrame, headerRect);
    [currentContext restoreGraphicsState];

    [currentContext saveGraphicsState];
    NSRectClip(footerRect);
    _webFrame->page()->drawFooter(*_webFrame, footerRect);
    [currentContext restoreGraphicsState];
}

- (NSRect)rectForPage:(NSInteger)page
{
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);
    if (![self _hasPageRects]) {
        LOG(Printing, "-[WKPrintingView %p rectForPage:%d] - data is not yet available", self, (int)page);
        if (!_webFrame->page()) {
            // We may have not told AppKit how many pages there are, so it will try to print until a null rect is returned.
            return NSZeroRect;
        }
        // We must be still calculating the page range.
        ASSERT(_expectedComputedPagesCallback);
        return NSMakeRect(0, 0, 1, 1);
    }

    // If Web process crashes while computing page rects, we never tell AppKit how many pages there are.
    // Returning a null rect prevents selecting non-existent pages in preview dialog.
    if (static_cast<unsigned>(page) > _printingPageRects.size()) {
        ASSERT(!_webFrame->page());
        return NSZeroRect;
    }

    WebCore::IntRect rect = _printingPageRects[page - 1];
    rect.scale(_totalScaleFactorForPrinting);
    LOG(Printing, "-[WKPrintingView %p rectForPage:%d] -> x %d, y %d, width %d, height %d", self, (int)page, rect.x(), rect.y(), rect.width(), rect.height());
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

    LOG(Printing, "-[WKPrintingView %p beginDocument]", self);

    [super beginDocument];

    [self _suspendAutodisplay];
}

- (void)endDocument
{
    ASSERT(_printOperation == [NSPrintOperation currentOperation]);

    // Forcing preview update gets us here, but page setup hasn't actually changed.
    if (isForcingPreviewUpdate)
        return;

    LOG(Printing, "-[WKPrintingView %p endDocument] - clearing cached data", self);

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

#endif // PLATFORM(MAC)
