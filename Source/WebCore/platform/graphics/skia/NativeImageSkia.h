/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#ifndef NativeImageSkia_h
#define NativeImageSkia_h

#include "SkBitmap.h"
#include "SkRect.h"
#include "IntSize.h"

namespace WebCore {

// This object is used as the "native image" in our port. When WebKit uses
// "NativeImagePtr", it is a pointer to this type. It has an SkBitmap, and also
// stores a cached resized image.
class NativeImageSkia {
public:
    NativeImageSkia();
    ~NativeImageSkia();

    // This constructor does a shallow copy of the passed-in SkBitmap (ie., it
    // references the same pixel data and bumps the refcount).  Use only when
    // you want sharing semantics.
    explicit NativeImageSkia(const SkBitmap&);

    // Returns the number of bytes of image data. This includes the cached
    // resized version if there is one.
    int decodedSize() const;

    // Sets the immutable flag on the bitmap, indicating that the image data
    // will not be modified any further. This is called by the image decoder
    // when all data is complete, used by us to know whether we can cache
    // resized images, and used by Skia for various optimizations.
    void setDataComplete() { m_image.setImmutable(); }

    // Returns true if the entire image has been decoded.
    bool isDataComplete() const { return m_image.isImmutable(); }

    // Get reference to the internal SkBitmap representing this image.
    const SkBitmap& bitmap() const { return m_image; }
    SkBitmap& bitmap() { return m_image; }

    // We can keep a resized version of the bitmap cached on this object.
    // This function will return true if there is a cached version of the
    // given image subset with the given dimensions and subsets.
    bool hasResizedBitmap(const SkIRect& srcSubset, int width, int height) const;

    // This will return an existing resized image subset, or generate a new one
    // of the specified size and subsets and possibly cache it.
    // srcSubset is the subset of the image to resize in image space.
    SkBitmap resizedBitmap(const SkIRect& srcSubset, int destWidth, int destHeight) const
    {
        SkIRect destVisibleSubset = {0, 0, destWidth, destHeight};
        return resizedBitmap(srcSubset, destWidth, destHeight, destVisibleSubset);
    }

    // Same as above, but returns a subset of the destination image (ie: the
    // visible subset). destVisibleSubset is the subset of the resized
    // (destWidth x destHeight) image.
    // In other words:
    // - crop image by srcSubset -> imageSubset.
    // - resize imageSubset to destWidth x destHeight -> destImage.
    // - return destImage cropped by destVisibleSubset.
    SkBitmap resizedBitmap(const SkIRect& srcSubset, int destWidth, int destHeight, const SkIRect& destVisibleSubset) const;

private:
    // CachedImageInfo is used to uniquely identify cached or requested image
    // resizes.
    struct CachedImageInfo {
        IntSize requestSize;
        SkIRect srcSubset;

        CachedImageInfo();

        bool isEqual(const SkIRect& otherSrcSubset, int width, int height) const;
        void set(const SkIRect& otherSrcSubset, int width, int height);
    };

    // Returns true if the given resize operation should either resize the whole
    // image and cache it, or resize just the part it needs and throw the result
    // away.
    //
    // Calling this function may increment a request count that can change the
    // result of subsequent calls.
    //
    // On the one hand, if only a small subset is desired, then we will waste a
    // lot of time resampling the entire thing, so we only want to do exactly
    // what's required. On the other hand, resampling the entire bitmap is
    // better if we're going to be using it more than once (like a bitmap
    // scrolling on and off the screen. Since we only cache when doing the
    // entire thing, it's best to just do it up front.
    bool shouldCacheResampling(const SkIRect& srcSubset,
                               int destWidth,
                               int destHeight,
                               const SkIRect& destSubset) const;

    // The original image.
    SkBitmap m_image;

    // The cached bitmap. This will be empty() if there is no cached image.
    mutable SkBitmap m_resizedImage;

    // References how many times that the image size has been requested for
    // the last size.
    //
    // Every time we get a call to shouldCacheResampling, if it matches the
    // m_cachedImageInfo, we'll increment the counter, and if not, we'll reset
    // the counter and save the dimensions.
    //
    // This allows us to see if many requests have been made for the same
    // resized image, we know that we should probably cache it, even if all of
    // those requests individually are small and would not otherwise be cached.
    //
    // We also track the source and destination subsets for caching partial
    // image resizes.
    mutable CachedImageInfo m_cachedImageInfo;
    mutable int m_resizeRequests;
};

}
#endif  // NativeImageSkia_h

