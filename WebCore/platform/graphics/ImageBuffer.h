/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "TransformationMatrix.h"
#include "Image.h"
#include "IntSize.h"
#include "ImageBufferData.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class GraphicsContext;
    class ImageData;
    class IntPoint;
    class IntRect;
    class String;

    class ImageBuffer : Noncopyable {
    public:
        // Will return a null pointer on allocation failure.
        static PassOwnPtr<ImageBuffer> create(const IntSize& size, bool grayScale)
        {
            bool success = false;
            OwnPtr<ImageBuffer> buf(new ImageBuffer(size, grayScale, success));
            if (success)
                return buf.release();
            return 0;
        }

        ~ImageBuffer();

        const IntSize& size() const { return m_size; }
        GraphicsContext* context() const;

        Image* image() const;

        void clearImage() { m_image.clear(); }

        PassRefPtr<ImageData> getImageData(const IntRect& rect) const;
        void putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint);

        String toDataURL(const String& mimeType) const;
#if !PLATFORM(CG)
        TransformationMatrix baseTransform() const { return TransformationMatrix(); }
#else
        TransformationMatrix baseTransform() const { return TransformationMatrix(1, 0, 0, -1, 0, m_size.height()); }
#endif
    private:
        ImageBufferData m_data;

        IntSize m_size;
        OwnPtr<GraphicsContext> m_context;
        mutable RefPtr<Image> m_image;

        // This constructor will place its success into the given out-variable
        // so that create() knows when it should return failure.
        ImageBuffer(const IntSize&, bool grayScale, bool& success);
    };

} // namespace WebCore

#endif // ImageBuffer_h
