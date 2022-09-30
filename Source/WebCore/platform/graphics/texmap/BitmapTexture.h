/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef BitmapTexture_h
#define BitmapTexture_h

#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class FilterOperations;
class GraphicsLayer;
class Image;
class TextureMapper;

// A 2D texture that can be the target of software or GL rendering.
class BitmapTexture : public RefCounted<BitmapTexture> {
public:
    enum Flag {
        NoFlag = 0,
        SupportsAlpha = 1 << 0,
        DepthBuffer = 1 << 1,
    };

    typedef unsigned Flags;

    BitmapTexture()
        : m_flags(0)
    {
    }

    virtual ~BitmapTexture() = default;
    virtual bool isBackedByOpenGL() const { return false; }

    virtual IntSize size() const = 0;
    virtual void updateContents(Image*, const IntRect&, const IntPoint& offset) = 0;
    void updateContents(GraphicsLayer*, const IntRect& target, const IntPoint& offset, float scale = 1);
    virtual void updateContents(const void*, const IntRect& target, const IntPoint& offset, int bytesPerLine) = 0;
    virtual bool isValid() const = 0;
    inline Flags flags() const { return m_flags; }

    virtual int bpp() const { return 32; }
    void reset(const IntSize& size, Flags flags = 0)
    {
        m_flags = flags;
        m_contentSize = size;
        didReset();
    }
    virtual void didReset() { }

    inline IntSize contentSize() const { return m_contentSize; }
    inline int numberOfBytes() const { return size().width() * size().height() * bpp() >> 3; }
    inline bool isOpaque() const { return !(m_flags & SupportsAlpha); }

    virtual RefPtr<BitmapTexture> applyFilters(TextureMapper&, const FilterOperations&, bool) { return this; }

protected:
    IntSize m_contentSize;

private:
    Flags m_flags;
};

}

#endif // BitmapTexture_h
