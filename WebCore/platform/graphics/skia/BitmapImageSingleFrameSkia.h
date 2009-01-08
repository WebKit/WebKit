// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    // Creates a new Image, by copying the pixel values out of |bitmap|.
    // If creation failed, returns null.
    static PassRefPtr<BitmapImageSingleFrameSkia> create(
        const SkBitmap& bitmap);

    virtual bool isBitmapImage() const { return true; }

    virtual IntSize size() const
    {
        return IntSize(m_nativeImage.width(), m_nativeImage.height());
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

protected:
    virtual void draw(GraphicsContext* ctxt, const FloatRect& dstRect,
                      const FloatRect& srcRect, CompositeOperator compositeOp);

private:
    NativeImageSkia m_nativeImage;

    // Use create().
    BitmapImageSingleFrameSkia() { }
};

} // namespace WebCore

#endif  // BitmapImageSingleFrameSkia_h
