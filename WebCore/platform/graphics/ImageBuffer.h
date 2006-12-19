/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef ImageBuffer_H
#define ImageBuffer_H

#include "IntSize.h"
#include <wtf/OwnPtr.h>

#if PLATFORM(CG)
typedef struct CGImage* CGImageRef;
#endif

namespace WebCore {

    class GraphicsContext;
    class RenderObject;

    class ImageBuffer {
    public:
        ImageBuffer(const IntSize&, GraphicsContext*);
        ~ImageBuffer();

        IntSize size() const;
        GraphicsContext* context() const;

        // This offers a way to render parts of a WebKit rendering tree into this ImageBuffer class.
        static void renderSubtreeToImage(ImageBuffer*, RenderObject* item);

#if PLATFORM(CG)
        CGImageRef cgImage() const;
#endif

    private:
        OwnPtr<GraphicsContext> m_context;

#if PLATFORM(CG) 
        IntSize m_size;
        mutable CGImageRef m_cgImage;
#endif
    };
}

#endif
