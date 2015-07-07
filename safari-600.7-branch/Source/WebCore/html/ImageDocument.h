/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2014 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ImageDocument_h
#define ImageDocument_h

#include "HTMLDocument.h"

namespace WebCore {

class ImageDocumentElement;
class HTMLImageElement;

class ImageDocument final : public HTMLDocument {
public:
    static PassRefPtr<ImageDocument> create(Frame& frame, const URL& url)
    {
        return adoptRef(new ImageDocument(frame, url));
    }

    HTMLImageElement* imageElement() const;

    void updateDuringParsing();
    void finishedParsing();

    void disconnectImageElement() { m_imageElement = nullptr; }

#if !PLATFORM(IOS)
    void windowSizeChanged();
    void imageClicked(int x, int y);
#endif

private:
    ImageDocument(Frame&, const URL&);

    virtual PassRefPtr<DocumentParser> createParser() override;

    LayoutSize imageSize();

    void createDocumentStructure();
#if !PLATFORM(IOS)
    void resizeImageToFit();
    void restoreImageSize();
    bool imageFitsInWindow();
    float scale();
#endif

    void imageUpdated();

    ImageDocumentElement* m_imageElement;

    // Whether enough of the image has been loaded to determine its size.
    bool m_imageSizeIsKnown;

#if !PLATFORM(IOS)
    // Whether the image is shrunk to fit or not.
    bool m_didShrinkImage;
#endif

    // Whether the image should be shrunk or not.
    bool m_shouldShrinkImage;
};

inline bool isImageDocument(const Document& document) { return document.isImageDocument(); }
void isImageDocument(const ImageDocument&); // Catch unnecessary runtime check of type known at compile time.

DOCUMENT_TYPE_CASTS(ImageDocument)

}

#endif // ImageDocument_h
