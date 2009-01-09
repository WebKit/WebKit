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
#include "IntSize.h"

// This object is used as the "native image" in our port. When WebKit uses
// "NativeImagePtr", it is a pointer to this type. It is an SkBitmap, but also
// stores a cached resized image.
class NativeImageSkia : public SkBitmap {
public:
    NativeImageSkia();

    // Returns the number of bytes of image data. This includes the cached
    // resized version if there is one.
    int decodedSize() const;

    // Sets the data complete flag. This is called by the image decoder when
    // all data is complete, and used by us to know whether we can cache
    // resized images.
    void setDataComplete() { m_isDataComplete = true; }

    // Returns true if the entire image has been decoded.
    bool isDataComplete() const { return m_isDataComplete; }

    // We can keep a resized version of the bitmap cached on this object.
    // This function will return true if there is a cached version of the
    // given image subset with the given dimensions.
    bool hasResizedBitmap(int width, int height) const;

    // This will return an existing resized image, or generate a new one of
    // the specified size and store it in the cache. Subsetted images can not
    // be cached unless the subset is the entire bitmap.
    SkBitmap resizedBitmap(int width, int height) const;

    // Returns true if the given resize operation should either resize the whole
    // image and cache it, or resize just the part it needs and throw the result
    // away.
    //
    // On the one hand, if only a small subset is desired, then we will waste a
    // lot of time resampling the entire thing, so we only want to do exactly
    // what's required. On the other hand, resampling the entire bitmap is
    // better if we're going to be using it more than once (like a bitmap
    // scrolling on and off the screen. Since we only cache when doing the
    // entire thing, it's best to just do it up front.
    bool shouldCacheResampling(int destWidth,
                               int destHeight,
                               int destSubsetWidth,
                               int destSubsetHeight) const;

private:
    // Set to true when the data is complete. Before the entire image has
    // loaded, we do not want to cache a resize.
    bool m_isDataComplete;

    // The cached bitmap. This will be empty() if there is no cached image.
    mutable SkBitmap m_resizedImage;

    // References how many times that the image size has been requested for
    // the last size.
    //
    // Every time we get a request, if it matches the m_lastRequestSize, we'll
    // increment the counter, and if not, we'll reset the counter and save the
    // size.
    //
    // This allows us to see if many requests have been made for the same
    // resized image, we know that we should probably cache it, even if all of
    // those requests individually are small and would not otherwise be cached.
    mutable WebCore::IntSize m_lastRequestSize;
    mutable int m_resizeRequests;
};

#endif  // NativeImageSkia_h

