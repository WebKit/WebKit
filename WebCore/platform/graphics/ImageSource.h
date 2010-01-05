/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef ImageSource_h
#define ImageSource_h

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#if PLATFORM(WX)
class wxBitmap;
class wxGraphicsBitmap;
#elif PLATFORM(CG)
typedef struct CGImageSource* CGImageSourceRef;
typedef struct CGImage* CGImageRef;
typedef const struct __CFData* CFDataRef;
#elif PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QPixmap;
QT_END_NAMESPACE
#elif PLATFORM(CAIRO)
struct _cairo_surface;
typedef struct _cairo_surface cairo_surface_t;
#elif PLATFORM(SKIA)
class NativeImageSkia;
#elif PLATFORM(HAIKU)
class BBitmap;
#elif OS(WINCE)
#include "SharedBitmap.h"
#endif

namespace WebCore {

class IntSize;
class SharedBuffer;
class String;

#if PLATFORM(CG)
typedef CGImageSourceRef NativeImageSourcePtr;
typedef CGImageRef NativeImagePtr;
#elif PLATFORM(QT)
class ImageDecoderQt;
typedef ImageDecoderQt* NativeImageSourcePtr;
typedef QPixmap* NativeImagePtr;
#else
class ImageDecoder;
typedef ImageDecoder* NativeImageSourcePtr;
#if PLATFORM(WX)
#if USE(WXGC)
typedef wxGraphicsBitmap* NativeImagePtr;
#else
typedef wxBitmap* NativeImagePtr;
#endif
#elif PLATFORM(CAIRO)
typedef cairo_surface_t* NativeImagePtr;
#elif PLATFORM(SKIA)
typedef NativeImageSkia* NativeImagePtr;
#elif PLATFORM(HAIKU)
typedef BBitmap* NativeImagePtr;
#elif OS(WINCE)
typedef RefPtr<SharedBitmap> NativeImagePtr;
#endif
#endif

const int cAnimationLoopOnce = -1;
const int cAnimationNone = -2;

class ImageSource : public Noncopyable {
public:
    ImageSource();
    ~ImageSource();

    // Tells the ImageSource that the Image no longer cares about decoded frame
    // data -- at all (if |destroyAll| is true), or before frame
    // |clearBeforeFrame| (if |destroyAll| is false).  The ImageSource should
    // delete cached decoded data for these frames where possible to keep memory
    // usage low.  When |destroyAll| is true, the ImageSource should also reset
    // any local state so that decoding can begin again.
    //
    // Implementations that delete less than what's specified above waste
    // memory.  Implementations that delete more may burn CPU re-decoding frames
    // that could otherwise have been cached, or encounter errors if they're
    // asked to decode frames they can't decode due to the loss of previous
    // decoded frames.
    //
    // Callers should not call clear(false, n) and subsequently call
    // createFrameAtIndex(m) with m < n, unless they first call clear(true).
    // This ensures that stateful ImageSources/decoders will work properly.
    //
    // The |data| and |allDataReceived| parameters should be supplied by callers
    // who set |destroyAll| to true if they wish to be able to continue using
    // the ImageSource.  This way implementations which choose to destroy their
    // decoders in some cases can reconstruct them correctly.
    void clear(bool destroyAll,
               size_t clearBeforeFrame = 0,
               SharedBuffer* data = NULL,
               bool allDataReceived = false);

    bool initialized() const;

    void setData(SharedBuffer* data, bool allDataReceived);
    String filenameExtension() const;

    bool isSizeAvailable();
    IntSize size() const;
    IntSize frameSizeAtIndex(size_t) const;

    int repetitionCount();

    size_t frameCount() const;

    // Callers should not call this after calling clear() with a higher index;
    // see comments on clear() above.
    NativeImagePtr createFrameAtIndex(size_t);

    float frameDurationAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t); // Whether or not the frame actually used any alpha.
    bool frameIsCompleteAtIndex(size_t); // Whether or not the frame is completely decoded.

private:
    NativeImageSourcePtr m_decoder;
};

}

#endif
