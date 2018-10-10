/*
 * Copyright (C) 2006, 2007, 2008, 2010, 2011 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebPDFViewPlaceholder.h"

#import "WebFrameInternal.h"
#import "WebPDFViewIOS.h"
#import <JavaScriptCore/JSContextRef.h>
#import <WebCore/DataTransfer.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FormState.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/MouseEvent.h>
#import <WebKitLegacy/WebDataSourcePrivate.h>
#import <WebKitLegacy/WebFramePrivate.h>
#import <WebKitLegacy/WebJSPDFDoc.h>
#import <WebKitLegacy/WebNSURLExtras.h>
#import <WebKitLegacy/WebNSViewExtras.h>
#import <WebKitLegacy/WebPDFDocumentExtras.h>
#import <WebKitLegacy/WebViewPrivate.h>
#import <wtf/MonotonicTime.h>
#import <wtf/SoftLinking.h>
#import <wtf/Vector.h>

using namespace WebCore;

#pragma mark Constants

static const float PAGE_WIDTH_INSET = 4.0f * 2.0f;
static const float PAGE_HEIGHT_INSET = 4.0f * 2.0f;

#pragma mark NSValue helpers

@interface NSValue (_web_Extensions)
+ (NSValue *)_web_valueWithCGRect:(CGRect)rect;
@end

@implementation NSValue (_web_Extensions)

+ (NSValue *)_web_valueWithCGRect:(CGRect)rect
{
    return [NSValue valueWithBytes:&rect objCType:@encode(CGRect)];
}

- (CGRect)CGRectValue
{
    CGRect result;
    [self getValue:&result];
    return result;
}

@end

#pragma mark -

@interface WebPDFViewPlaceholder ()

- (void)_evaluateJSForDocument:(CGPDFDocumentRef)document;
- (void)_updateTitleForDocumentIfAvailable;
- (void)_updateTitleForURL:(NSURL *)URL;
- (CGSize)_computePageRects:(CGPDFDocumentRef)document;

@property (retain) NSArray *pageRects;
@property (retain) NSArray *pageYOrigins;
@property (retain) NSString *title;

@end

#pragma mark WebPDFViewPlaceholder implementation

@implementation WebPDFViewPlaceholder {
    NSString *_title;
    NSArray *_pageRects;
    NSArray *_pageYOrigins;
    CGPDFDocumentRef _document;
    WebDataSource *_dataSource; // weak to prevent cycles.
    
    NSObject<WebPDFViewPlaceholderDelegate> *_delegate;
    
    BOOL _didFinishLoad;
    
    CGSize _containerSize;
}

@synthesize delegate = _delegate;
@synthesize pageRects = _pageRects;
@synthesize pageYOrigins = _pageYOrigins;
@synthesize document = _document;
@synthesize title = _title;
@synthesize containerSize = _containerSize;

- (CGPDFDocumentRef)document
{
    @synchronized(self) {
        if (_document)
            return _document;
    }

    if ([self.delegate respondsToSelector:@selector(cgPDFDocument)])
        return [self.delegate cgPDFDocument];

    return nil;
}

- (void)setDocument:(CGPDFDocumentRef)document
{
    @synchronized(self) {
        CGPDFDocumentRetain(document);
        CGPDFDocumentRelease(_document);
        _document = document;
    }
}

- (void)clearDocument
{
    [self setDocument:nil];
}

- (CGPDFDocumentRef)doc
{
    return [self document];
}

- (NSUInteger)totalPages
{
    return CGPDFDocumentGetNumberOfPages([self document]);
}

+ (void)setAsPDFDocRepAndView
{
    [WebView _setPDFRepresentationClass:[WebPDFViewPlaceholder class]];
    [WebView _setPDFViewClass:[WebPDFViewPlaceholder class]];
}

// This is a secret protocol for WebDataSource and WebFrameView to offer us the opportunity to do something different 
// depending upon which frame is trying to instantiate a representation for PDF.
+ (Class)_representationClassForWebFrame:(WebFrame *)webFrame
{
    return [WebPDFView _representationClassForWebFrame:webFrame];
}

- (void)dealloc
{
    if ([self.delegate respondsToSelector:@selector(viewWillClose)])
        [self.delegate viewWillClose];

    [self setTitle:nil];

    [self setPageRects:nil];
    [self setPageYOrigins:nil];

    [self setDocument:nil];
    [super dealloc];
}

#pragma mark WebPDFDocumentView and WebPDFDocumentRepresentation protocols

+ (NSArray *)supportedMIMETypes
{
    return [WebPDFView supportedMIMETypes];
}

#pragma mark WebDocumentView and WebDocumentRepresentation protocols

- (void)setDataSource:(WebDataSource *)dataSource
{
    [self dataSourceUpdated:dataSource];

    if ([dataSource request])
        [self _updateTitleForURL:[[dataSource request] URL]];

    WAKView *superview = [self superview];
    if (superview)
        [self setBoundsSize:[self convertRect:[superview bounds] fromView:superview].size];
}

#pragma mark WebPDFViewPlaceholderDelegate stuff

- (void)_notifyDidCompleteLayout
{
    if ([NSThread isMainThread]) {
        if ([self.delegate respondsToSelector:@selector(didCompleteLayout)])
            [self.delegate didCompleteLayout];
    } else
        [self performSelectorOnMainThread:@selector(_notifyDidCompleteLayout) withObject:nil waitUntilDone:NO];
}

#pragma mark WebDocumentView protocol

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
    _dataSource = dataSource;
}

- (void)layout
{
    if (!_didFinishLoad)
        return;

    if (self.pageRects)
        return;

    CGSize boundingSize = [self _computePageRects:_document];

    [self setBoundsSize:boundingSize];

    _didCompleteLayout = YES;
    [self _notifyDidCompleteLayout];
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
}

- (void)viewDidMoveToHostWindow
{
}

#pragma mark WebDocumentRepresentation protocol

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
}

- (void)_doPostLoadOrUnlockTasks
{
    if (!_document)
        return;

    [self _updateTitleForDocumentIfAvailable];
    [self _evaluateJSForDocument:_document];

    // Any remaining work on the document should be done before this call to -layout,
    // which will hand ownership of the document to the delegate (UIWebPDFView) and
    // release the placeholder's document.
    [self layout];
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    [self dataSourceUpdated:dataSource];

    _didFinishLoad = YES;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((CFDataRef)[dataSource data]);
    if (!provider)
        return;

    _document = CGPDFDocumentCreateWithProvider(provider);

    CGDataProviderRelease(provider);

    [self _doPostLoadOrUnlockTasks];
}

- (BOOL)canProvideDocumentSource
{
    return NO;
}

- (NSString *)documentSource
{
    return nil;
}

#pragma mark internal stuff

- (void)_evaluateJSForDocument:(CGPDFDocumentRef)pdfDocument
{
    if (!pdfDocument || !CGPDFDocumentIsUnlocked(pdfDocument))
        return;

    NSArray *scripts = allScriptsInPDFDocument(pdfDocument);

    if ([scripts count]) {
        JSGlobalContextRef ctx = JSGlobalContextCreate(0);
        JSObjectRef jsPDFDoc = makeJSPDFDoc(ctx, _dataSource);
        for (NSString *script in scripts)
            JSEvaluateScript(ctx, OpaqueJSString::tryCreate(script).get(), jsPDFDoc, nullptr, 0, nullptr);
        JSGlobalContextRelease(ctx);
    }
}

- (void)_updateTitleForURL:(NSURL *)URL
{
    NSString *titleFromURL = [URL lastPathComponent];
    if (![titleFromURL length] || [titleFromURL isEqualToString:@"/"])
        titleFromURL = [[URL _web_hostString] _webkit_decodeHostName];

    [self setTitle:titleFromURL];
    [[self _frame] _dispatchDidReceiveTitle:titleFromURL];
}

- (void)_updateTitleForDocumentIfAvailable
{
    if (!_document || !CGPDFDocumentIsUnlocked(_document))
        return;

    NSString *title = nil;

    CGPDFDictionaryRef info = CGPDFDocumentGetInfo(_document);
    CGPDFStringRef value;
    if (CGPDFDictionaryGetString(info, "Title", &value))
        title = (NSString *)CGPDFStringCopyTextString(value);

    if ([title length]) {
        [self setTitle:title];
        [[self _frame] _dispatchDidReceiveTitle:title];
    }

    [title release];
}

- (CGRect)_getPDFPageBounds:(CGPDFPageRef)page
{    
    if (!page)
        return CGRectZero;

    CGRect bounds = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
    CGFloat rotation = CGPDFPageGetRotationAngle(page) * (M_PI / 180.0f);
    if (rotation != 0)
        bounds = CGRectApplyAffineTransform(bounds, CGAffineTransformMakeRotation(rotation));

    bounds.origin.x     = roundf(bounds.origin.x);
    bounds.origin.y     = roundf(bounds.origin.y);
    bounds.size.width   = roundf(bounds.size.width);
    bounds.size.height  = roundf(bounds.size.height);

    return bounds;
}

- (CGSize)_computePageRects:(CGPDFDocumentRef)pdfDocument
{
    // Do we even need to compute the page rects?
    if (self.pageRects)
        return [self bounds].size;

    if (!pdfDocument)
        return CGSizeZero;

    if (!CGPDFDocumentIsUnlocked(pdfDocument))
        return CGSizeZero;

    size_t pageCount = CGPDFDocumentGetNumberOfPages(pdfDocument);
    if (!pageCount)
        return CGSizeZero;

    NSMutableArray *pageRects = [NSMutableArray array];
    if (!pageRects)
        return CGSizeZero;

    NSMutableArray *pageYOrigins = [NSMutableArray array];
    if (!pageYOrigins)
        return CGSizeZero;

    // Temporary vector to avoid getting the page rects twice.
    WTF::Vector<CGRect> pageCropBoxes;

    // First we need to determine the width of the widest page.
    CGFloat maxPageWidth = 0.0f;

    // CG uses page numbers instead of page indices, so 1 based.
    for (size_t i = 1; i <= pageCount; i++) {
        CGPDFPageRef page = CGPDFDocumentGetPage(pdfDocument, i);
        if (!page) {
            // So if there is a missing page, then effectively the document ends here.
            // See <rdar://problem/10428152> iOS Mail crashes when opening a PDF
            pageCount = i - 1;
            break;
        }

        CGRect pageRect = [self _getPDFPageBounds:page];

        maxPageWidth = std::max<CGFloat>(maxPageWidth, pageRect.size.width);

        pageCropBoxes.append(pageRect);
    }

    if (!pageCount)
        return CGSizeZero;

    CGFloat scalingFactor = _containerSize.width / maxPageWidth;
    if (_containerSize.width < FLT_EPSILON)
        scalingFactor = 1.0;

    // Including the margins, what is the desired width of the content?
    CGFloat desiredWidth = (maxPageWidth + (PAGE_WIDTH_INSET * 2.0f)) * scalingFactor;
    CGFloat desiredHeight = PAGE_HEIGHT_INSET;

    for (size_t i = 0; i < pageCount; i++) {

        CGRect pageRect = pageCropBoxes[i];

        // Apply our scaling to this page's bounds.
        pageRect.size.width  = roundf(pageRect.size.width * scalingFactor);
        pageRect.size.height = roundf(pageRect.size.height * scalingFactor);

        // Center this page horizontally and update the starting vertical offset.
        pageRect.origin.x = roundf((desiredWidth - pageRect.size.width) / 2.0f);
        pageRect.origin.y = desiredHeight;

        // Save this page's rect and minimum y-offset.
        [pageRects addObject:[NSValue _web_valueWithCGRect:pageRect]];
        [pageYOrigins addObject:[NSNumber numberWithFloat:CGRectGetMinY(pageRect)]];

        // Update the next starting vertical offset.
        desiredHeight += pageRect.size.height + PAGE_HEIGHT_INSET;
    }

    // Save the resulting page rects array and minimum y-offsets array.
    self.pageRects = pageRects;
    self.pageYOrigins = pageYOrigins;

    // Determine the result desired bounds.
    return CGSizeMake(roundf(desiredWidth), roundf(desiredHeight));
}

- (void)didUnlockDocument
{
    [self _doPostLoadOrUnlockTasks];
}

- (CGRect)rectForPageNumber:(NSUInteger)pageNumber
{
    // Page number is 1-based, not 0 based.
    if ((!pageNumber) || (pageNumber > [_pageRects count]))
        return CGRectNull;

    return [[_pageRects objectAtIndex:pageNumber - 1] CGRectValue];
}

- (void)simulateClickOnLinkToURL:(NSURL *)URL
{
    if (!URL)
        return;

    RefPtr<Event> event = MouseEvent::create(eventNames().clickEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, Event::IsComposed::Yes,
        MonotonicTime::now(), nullptr, 1, { }, { }, { }, { }, 0, 0, nullptr, 0, 0, nullptr, MouseEvent::IsSimulated::Yes);

    // Call to the frame loader because this is where our security checks are made.
    Frame* frame = core([_dataSource webFrame]);
    FrameLoadRequest frameLoadRequest { *frame->document(), frame->document()->securityOrigin(), { URL }, { }, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ShouldOpenExternalURLsPolicy::ShouldNotAllow,  InitiatedByMainFrame::Unknown };
    frame->loader().loadFrameRequest(WTFMove(frameLoadRequest), event.get(), nullptr);
}

@end

#endif /* PLATFORM(IOS) */
