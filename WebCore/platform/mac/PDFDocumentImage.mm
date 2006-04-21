/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "Image.h"
#import "PDFDocumentImage.h"

namespace WebCore {

static void releasePDFDocumentData(void *info, const void *data, size_t size)
{
    [(id)info autorelease];
}

PDFDocumentImage::PDFDocumentImage(NSData* data)
{
    if (data != nil) {
        CGDataProviderRef dataProvider = CGDataProviderCreateWithData([data retain], [data bytes], [data length], releasePDFDocumentData);
        m_document = CGPDFDocumentCreateWithProvider(dataProvider);
        CGDataProviderRelease(dataProvider);
    }
    m_currentPage = -1;
    setCurrentPage(0);
}

PDFDocumentImage::~PDFDocumentImage()
{
    CGPDFDocumentRelease(m_document);
}

CGPDFDocumentRef PDFDocumentImage::documentRef()
{
    return m_document;
}

CGRect PDFDocumentImage::mediaBox()
{
    return m_mediaBox;
}

CGRect PDFDocumentImage::bounds()
{
    CGRect rotatedRect;

    // rotate the media box and calculate bounding box
    float sina   = sinf(m_rotation);
    float cosa   = cosf(m_rotation);
    float width  = m_cropBox.size.width;
    float height = m_cropBox.size.height;

    // calculate rotated x and y axis
    NSPoint rx = NSMakePoint( width  * cosa, width  * sina);
    NSPoint ry = NSMakePoint(-height * sina, height * cosa);

    // find delta width and height of rotated points
    rotatedRect.origin      = m_cropBox.origin;
    rotatedRect.size.width  = ceilf(fabs(rx.x - ry.x));
    rotatedRect.size.height = ceilf(fabs(ry.y - rx.y));

    return rotatedRect;
}

void PDFDocumentImage::adjustCTM(CGContextRef context)
{
    // rotate the crop box and calculate bounding box
    float sina   = sinf(-m_rotation);
    float cosa   = cosf(-m_rotation);
    float width  = m_cropBox.size.width;
    float height = m_cropBox.size.height;

    // calculate rotated x and y edges of the corp box. if they're negative, it means part of the image has
    // been rotated outside of the bounds and we need to shift over the image so it lies inside the bounds again
    NSPoint rx = NSMakePoint( width  * cosa, width  * sina);
    NSPoint ry = NSMakePoint(-height * sina, height * cosa);

    // adjust so we are at the crop box origin
    CGContextTranslateCTM(context, floorf(-MIN(0,MIN(rx.x, ry.x))), floorf(-MIN(0,MIN(rx.y, ry.y))));

    // rotate -ve to remove rotation
    CGContextRotateCTM(context, -m_rotation);

    // shift so we are completely within media box
    CGContextTranslateCTM(context, m_mediaBox.origin.x - m_cropBox.origin.x, m_mediaBox.origin.y - m_cropBox.origin.y);
}

void PDFDocumentImage::setCurrentPage(int page)
{
    if (page != m_currentPage && page >= 0 && page < pageCount()) {

        CGRect r;

        m_currentPage = page;

        // get media box (guaranteed)
        m_mediaBox = CGPDFDocumentGetMediaBox(m_document, page + 1);

        // get crop box (not always there). if not, use _mediaBox
        r = CGPDFDocumentGetCropBox(m_document, page + 1);
        if (!CGRectIsEmpty(r)) {
            m_cropBox = CGRectMake(r.origin.x, r.origin.y, r.size.width, r.size.height);
        } else {
            m_cropBox = CGRectMake(m_mediaBox.origin.x, m_mediaBox.origin.y, m_mediaBox.size.width, m_mediaBox.size.height);
        }

        // get page rotation angle
        m_rotation = CGPDFDocumentGetRotationAngle(m_document, page + 1) * M_PI / 180.0; // to radians
    }
}

int PDFDocumentImage::currentPage()
{
    return m_currentPage;
}

int PDFDocumentImage::pageCount()
{
    return CGPDFDocumentGetNumberOfPages(m_document);
}

void PDFDocumentImage::draw(NSRect srcRect, NSRect dstRect, CompositeOperator op, 
                            float alpha, bool flipped, CGContextRef context)
{
    CGContextSaveGState(context);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] setCompositingOperation:(NSCompositingOperation)op];
    [pool release];

    // Scale and translate so the document is rendered in the correct location.
    float hScale = dstRect.size.width  / srcRect.size.width;
    float vScale = dstRect.size.height / srcRect.size.height;

    CGContextTranslateCTM(context, dstRect.origin.x - srcRect.origin.x * hScale, dstRect.origin.y - srcRect.origin.y * vScale);
    CGContextScaleCTM(context, hScale, vScale);

    // Reverse if flipped image.
    if (flipped) {
        CGContextScaleCTM(context, 1, -1);
        CGContextTranslateCTM(context, 0, -dstRect.size.height);
    }

    // Clip to destination in case we are imaging part of the source only
    CGContextClipToRect(context, CGRectIntegral(*(CGRect*)&srcRect));

    // and draw
    if (m_document) {
        CGContextSaveGState(context);
        // Rotate translate image into position according to doc properties.
        adjustCTM(context);

        // Media box may have non-zero origin which we ignore. CGPDFDocumentShowPage pages start
        // at 1, not 0.
        CGContextDrawPDFDocument(context, CGRectMake(0, 0, m_mediaBox.size.width, m_mediaBox.size.height), m_document, 1);

        CGContextRestoreGState(context);
    }

    // done with our fancy transforms
    CGContextRestoreGState(context);
}

}
