/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "IntSize.h"
#include <wtf/OwnPtr.h>
#include <memory>

#if PLATFORM(CG)
typedef struct CGImage* CGImageRef;
#endif

#if PLATFORM(QT)
#include <QPixmap>
class QPainter;
#endif

#if PLATFORM(CAIRO)
typedef struct _cairo_surface cairo_surface_t;
#endif

namespace WebCore {

    class GraphicsContext;
    class RenderObject;

    class ImageBuffer : Noncopyable {
    public:
        static std::auto_ptr<ImageBuffer> create(const IntSize&, bool grayScale);
        ~ImageBuffer();

        IntSize size() const { return m_size; }
        GraphicsContext* context() const;

#if PLATFORM(CG)
        CGImageRef cgImage() const;
#elif PLATFORM(QT)
        QPixmap* pixmap() const;
#elif PLATFORM(CAIRO)
        cairo_surface_t* surface() const;
#endif

    private:
        void* m_data;
        IntSize m_size;

        OwnPtr<GraphicsContext> m_context;

#if PLATFORM(CG)
        ImageBuffer(void* imageData, const IntSize&, std::auto_ptr<GraphicsContext>);
        mutable CGImageRef m_cgImage;
#elif PLATFORM(QT)
        ImageBuffer(const QPixmap &px);
        mutable QPixmap m_pixmap;
        mutable QPainter* m_painter;
#elif PLATFORM(CAIRO)
        ImageBuffer(cairo_surface_t* surface);
        mutable cairo_surface_t* m_surface;
#endif
    };
}

#endif
