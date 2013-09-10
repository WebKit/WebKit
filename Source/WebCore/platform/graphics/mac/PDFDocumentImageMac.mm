/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PDFDocumentImage.h"

#if USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)

#import "LocalCurrentGraphicsContext.h"
#import "SharedBuffer.h"
#import "SoftLinking.h"
#import <Quartz/Quartz.h>
#import <objc/objc-class.h>
#import <wtf/RetainPtr.h>

#ifdef __has_include
#if __has_include(<ApplicationServices/ApplicationServicesPriv.h>)
#import <ApplicationServices/ApplicationServicesPriv.h>
#endif
#endif

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, PDFKit)
SOFT_LINK_CLASS(PDFKit, PDFDocument)

extern "C" {
bool CGContextGetAllowsFontSmoothing(CGContextRef context);
bool CGContextGetAllowsFontSubpixelQuantization(CGContextRef context);
}

namespace WebCore {

void PDFDocumentImage::createPDFDocument()
{
    m_document = adoptNS([[getPDFDocumentClass() alloc] initWithData:data()->createNSData()]);
}

void PDFDocumentImage::computeBoundsForCurrentPage()
{
    PDFPage *pdfPage = [m_document pageAtIndex:0];

    m_mediaBox = [pdfPage boundsForBox:kPDFDisplayBoxMediaBox];
    m_cropBox = [pdfPage boundsForBox:kPDFDisplayBoxCropBox];

    m_rotation = deg2rad(static_cast<float>([pdfPage rotation]));
}

unsigned PDFDocumentImage::pageCount() const
{
    return [m_document pageCount];
}

void PDFDocumentImage::drawPDFPage(GraphicsContext* context)
{
    CGContextTranslateCTM(context->platformContext(), -m_cropBox.x(), -m_cropBox.y());

    LocalCurrentGraphicsContext localCurrentContext(context);

    // These states can be mutated by PDFKit but are not saved
    // on the context's state stack. (<rdar://problem/14951759>)
    bool allowsSmoothing = CGContextGetAllowsFontSmoothing(context->platformContext());
    bool allowsSubpixelQuantization = CGContextGetAllowsFontSubpixelQuantization(context->platformContext());

    [[m_document pageAtIndex:0] drawWithBox:kPDFDisplayBoxCropBox];

    CGContextSetAllowsFontSmoothing(context->platformContext(), allowsSmoothing);
    CGContextSetAllowsFontSubpixelQuantization(context->platformContext(), allowsSubpixelQuantization);
}

}

#endif // USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)
