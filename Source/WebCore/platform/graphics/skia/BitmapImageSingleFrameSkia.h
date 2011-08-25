/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BitmapImageSingleFrameSkia_h
#define BitmapImageSingleFrameSkia_h

#include "Image.h"
#include "NativeImageSkia.h"

namespace WebCore {

// This image class can be used in places which need an Image, but have
// raw pixel data rather than undecoded image data.
// The Image is simpler than a BitmapImage, as it does not have image
// observers, animation, multiple frames, or non-decoded data.
// Therefore trimming the decoded data (destroyDecodedData()) has no effect.
//
// The difficulty with putting this in BitmapImage::create(NativeImagePtr)
// is that NativeImagePtr = NativeImageSkia, yet callers have SkBitmap.
class BitmapImageSingleFrameSkia : public Image {
public:
    // Creates a new Image from the given SkBitmap.  If "copyPixels" is true, a
    // deep copy is done.  Otherwise, a shallow copy is done (pixel data is
    // ref'ed).
    static PassRefPtr<BitmapImageSingleFrameSkia> create(const SkBitmap&, bool copyPixels);

    virtual bool isBitmapImage() const { return true; }

    virtual bool currentFrameHasAlpha() { return !m_nativeImage.bitmap().isOpaque(); }

    virtual IntSize size() const
    {
        return IntSize(m_nativeImage.bitmap().width(), m_nativeImage.bitmap().height());
    }

    // Do nothing, as we only have the one representation of data (decoded).
    virtual void destroyDecodedData(bool destroyAll = true) { }

    virtual unsigned decodedSize() const
    {
        return m_nativeImage.decodedSize();
    }

    // We only have a single frame.
    virtual NativeImagePtr nativeImageForCurrentFrame()
    {
        return &m_nativeImage;
    }

#if !ASSERT_DISABLED
    virtual bool notSolidColor()
    {
        return m_nativeImage.bitmap().width() != 1 || m_nativeImage.bitmap().height() != 1;
    }
#endif

protected:
    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace styleColorSpace, CompositeOperator);

private:
    NativeImageSkia m_nativeImage;

    // Creates a new Image from the given SkBitmap, using a shallow copy.
    explicit BitmapImageSingleFrameSkia(const SkBitmap&);
};

FloatRect normalizeRect(const FloatRect&);

} // namespace WebCore

#endif  // BitmapImageSingleFrameSkia_h
