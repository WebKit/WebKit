/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef ImageDecodingStore_h
#define ImageDecodingStore_h

#include "SkBitmap.h"
#include "SkRect.h"
#include "SkSize.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class ImageDecoder;
class ImageFrameGenerator;
class ScaledImageFragment;
class SharedBuffer;

// This class can only be instantiated on main thread. There will be an
// instance on impl thread in the future but that part of implementation
// is incomplete. See bug: https://bugs.webkit.org/show_bug.cgi?id=94240
// for details.
class ImageDecodingStore {
public:
    static PassOwnPtr<ImageDecodingStore> create() { return adoptPtr(new ImageDecodingStore); }
    ~ImageDecodingStore();

    static ImageDecodingStore* instanceOnMainThread();
    static void initializeOnMainThread();
    static void shutdown();
    static bool isLazyDecoded(const SkBitmap&);

    SkBitmap createLazyDecodedSkBitmap(PassOwnPtr<ImageDecoder>);
    SkBitmap resizeLazyDecodedSkBitmap(const SkBitmap&, const SkISize& scaledSize, const SkIRect& scaledSubset);

    void setData(const SkBitmap&, PassRefPtr<SharedBuffer>, bool allDataReceived);

    // You may only lock one set of pixels at a time; overlapping lock calls are not allowed.
    void* lockPixels(PassRefPtr<ImageFrameGenerator>, const SkISize& scaledSize, const SkIRect& scaledSubset);
    void unlockPixels();

    void frameGeneratorBeingDestroyed(ImageFrameGenerator*);

private:
    ImageDecodingStore();
    bool calledOnValidThread() const;
    void createFrameGenerator(PassOwnPtr<ImageDecoder>);
    ScaledImageFragment* lookupFrameCache(int imageId, const SkISize& scaledSize) const;
    void deleteFrameCache(int imageId);

    Vector<OwnPtr<ScaledImageFragment> > m_frameCache;

    SkBitmap m_lockedSkBitmap;
};

} // namespace WebCore

#endif
