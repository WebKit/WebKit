/*
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Computer, Inc.  All rights reserved.
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

#ifndef PDFDocumentImage_h
#define PDFDocumentImage_h

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsTypes.h"
#include "Image.h"

#if USE(CG)

#if PLATFORM(MAC) && !PLATFORM(IOS)
#define WTF_USE_PDFKIT_FOR_PDFDOCUMENTIMAGE 1
#endif

typedef struct CGPDFDocument *CGPDFDocumentRef;
OBJC_CLASS PDFDocument;

namespace WebCore {

class GraphicsContext;
class ImageBuffer;

class PDFDocumentImage FINAL : public Image {
public:
    static PassRefPtr<PDFDocumentImage> create(ImageObserver* observer)
    {
        return adoptRef(new PDFDocumentImage(observer));
    }

private:
    PDFDocumentImage(ImageObserver*);
    virtual ~PDFDocumentImage();

    virtual bool isPDFDocumentImage() const OVERRIDE { return true; }

    virtual String filenameExtension() const OVERRIDE;

    virtual bool hasSingleSecurityOrigin() const OVERRIDE { return true; }

    virtual bool dataChanged(bool allDataReceived) OVERRIDE;

    virtual void destroyDecodedData(bool /*destroyAll*/ = true) OVERRIDE;
    virtual unsigned decodedSize() const OVERRIDE;

    virtual void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) OVERRIDE;
    virtual IntSize size() const OVERRIDE;

    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace styleColorSpace, CompositeOperator, BlendMode) OVERRIDE;

    // FIXME: Implement this to be less conservative.
    virtual bool currentFrameKnownToBeOpaque() OVERRIDE { return false; }

    void createPDFDocument();
    void computeBoundsForCurrentPage();
    unsigned pageCount() const;
    void drawPDFPage(GraphicsContext*);

    void updateCachedImageIfNeeded(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect);
    bool cacheParametersMatch(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect) const;

#if USE(PDFKIT_FOR_PDFDOCUMENTIMAGE)
    RetainPtr<PDFDocument> m_document;
#else
    RetainPtr<CGPDFDocumentRef> m_document;
#endif

    OwnPtr<ImageBuffer> m_cachedImageBuffer;
    AffineTransform m_cachedTransform;
    FloatSize m_cachedDestinationSize;
    FloatRect m_cachedSourceRect;
    size_t m_cachedBytes;

    FloatRect m_mediaBox;
    FloatRect m_cropBox;
    int m_rotationDegrees; // Can only be 0, 90, 180, or 270 degrees.
    bool m_hasPage;
};

}

#endif // USE(CG)

#endif // PDFDocumentImage_h
