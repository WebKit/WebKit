/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include "Image.h"
#include "IntSize.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <memory>

namespace WebCore {

    class GraphicsContext;
    class ImageData;
    class IntPoint;
    class IntRect;
    class RenderObject;
    class String;

    class ImageBuffer : Noncopyable {
    public:
        static std::auto_ptr<ImageBuffer> create(const IntSize&, bool grayScale);
        ~ImageBuffer();

        IntSize size() const { return m_size; }
        GraphicsContext* context() const;

        Image* image() const;

        void clearImage() { m_image.clear(); }

        PassRefPtr<ImageData> getImageData(const IntRect& rect) const;
        void putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint);

        String toDataURL(const String& mimeType) const;

    private:
        void* m_data;
        IntSize m_size;

        OwnPtr<GraphicsContext> m_context;
        mutable RefPtr<Image> m_image;
    
#if PLATFORM(CG)
        ImageBuffer(void* imageData, const IntSize&, std::auto_ptr<GraphicsContext>);
#elif PLATFORM(QT)
        ImageBuffer(const QPixmap&);
        mutable QPixmap m_pixmap;
        mutable QPainter* m_painter;
#elif PLATFORM(CAIRO)
        ImageBuffer(cairo_surface_t*);
        mutable cairo_surface_t* m_surface;
#endif
    };

} // namespace WebCore

#endif // ImageBuffer_h
