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

#import <WebKit/WebDocumentPrivate.h>

#if TARGET_OS_IPHONE

@class UIPDFDocument;
@protocol WebPDFViewPlaceholderDelegate;

/*!
    @class WebPDFViewPlaceholder
    @discussion This class represents a placeholder for PDFs. It is intended to allow main frame PDFs
    be drawn to the UI by some other object (ideally the delegate of this class) while still interfacing
    with WAK and WebKit correctly.
*/
@interface WebPDFViewPlaceholder : WAKView <WebPDFDocumentView, WebPDFDocumentRepresentation> {
    NSString *_title;
    NSArray *_pageRects;
    NSArray *_pageYOrigins;
    CGPDFDocumentRef _document;
    WebDataSource *_dataSource; // weak to prevent cycles.

    NSObject<WebPDFViewPlaceholderDelegate> *_delegate;

    BOOL _didFinishLoadAndMemoryMap;

    CGSize _containerSize;
}

/*!
    @method setAsPDFDocRepAndView
    @abstract This methods sets [WebPDFViewPlaceholder class] as the document and view representations
    for PDF.
*/
+ (void)setAsPDFDocRepAndView;


/*!
 @property delegate
 @abstract A delegate object conforming to WebPDFViewPlaceholderDelegate that will be informed about various state changes.
 */
@property (assign) NSObject<WebPDFViewPlaceholderDelegate> *delegate;

/*!
 @property pageRects
 @abstract An array of CGRects (as NSValues) representing the bounds of each page in PDF document coordinate space.
 */
@property (readonly, retain) NSArray *pageRects;

/*!
 @property pageYOrigins
 @abstract An array of CGFloats (as NSNumbers) representing the minimum y for every page in the document.
 */
@property (readonly, retain) NSArray *pageYOrigins;

/*!
 @property document
 @abstract The CGPDFDocumentRef that this object represents. Until the document has loaded, this property will be NULL.
 */
@property (readonly) CGPDFDocumentRef document;
@property (readonly) CGPDFDocumentRef doc;

/*!
 @property totalPages
 @abstract Convenience access for the total number of pages in the wrapped document.
 */
@property (readonly) NSUInteger totalPages;

/*!
 @property title
 @abstract PDFs support a meta data field for the document's title. If this field is present in the PDF, title will be that string. 
 If not, title will be the file name.
 */
@property (readonly, retain) NSString *title;

/*!
 @property containerSize
 @abstract sets the size for the containing view. This is used to determine how big the shadows between pages should be.
 */
@property (assign) CGSize containerSize;

@property (nonatomic, readonly) BOOL didCompleteLayout;

- (void)clearDocument;

/*!
 @method didUnlockDocument
 @abstract Informs the PDF view placeholder that the PDF document has been unlocked. The result of this involves laying 
 out the pages, retaining the document title, and re-evaluating the document's javascript. This should be called on the WebThread.
 */
- (void)didUnlockDocument;

/*!
 @method rectForPageNumber
 @abstract Returns the PDF document coordinate space rect given a page number. pageNumber must be in the range [1,totalPages], 
 since page numbers are 1-based.
 */
- (CGRect)rectForPageNumber:(NSUInteger)pageNumber;

/*!
 @method simulateClickOnLinkToURL:
 @abstract This method simulates a user clicking on the passed in URL.
 */
- (void)simulateClickOnLinkToURL:(NSURL *)URL;

@end


/*!
    @protocol WebPDFViewPlaceholderDelegate
    @discussion This protocol is used to inform the object using the placeholder that the layout for the 
    document has been calculated.
*/
@protocol WebPDFViewPlaceholderDelegate

@optional

/*!
 @method viewWillClose
 @abstract This method is called to inform the delegate that the placeholder view's lifetime has ended. This might
 be called from either the main thread or the WebThread.
 */
- (void)viewWillClose;

/*!
    @method didCompleteLayout
    @abstract This method is called to inform the delegate that the placeholder has completed layout
    and determined the document's bounds. Will always be called on the main thread.
*/
- (void)didCompleteLayout;

@required

/*!
 @method cgPDFDocument
 @abstract The WebPDFViewPlaceholder may have abdicated responsibility for the underlying CGPDFDocument to the WebPDFViewPlaceholderDelegate.
 That means that there may be times when the document is needed, but the WebPDFViewPlaceholder no longer has a reference to it. In which case
 the WebPDFViewPlaceholderDelegate will be asked for the document.
 */
- (CGPDFDocumentRef)cgPDFDocument;

@end

#endif /* TARGET_OS_IPHONE */
