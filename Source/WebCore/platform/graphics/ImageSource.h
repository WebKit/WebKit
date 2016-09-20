/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010, 2012, 2014, 2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "ImageFrame.h"
#include "ImageOrientation.h"
#include "IntPoint.h"
#include "NativeImage.h"
#include "TextStream.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>

namespace WebCore {

class ImageOrientation;
class IntPoint;
class IntSize;
class SharedBuffer;
class ImageDecoder;

class ImageSource {
    WTF_MAKE_NONCOPYABLE(ImageSource);
    friend class BitmapImage;
public:
    enum AlphaOption {
        AlphaPremultiplied,
        AlphaNotPremultiplied
    };

    enum GammaAndColorProfileOption {
        GammaAndColorProfileApplied,
        GammaAndColorProfileIgnored
    };

    ImageSource(const NativeImagePtr&);
    ImageSource(AlphaOption = AlphaPremultiplied, GammaAndColorProfileOption = GammaAndColorProfileApplied);
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
    // createFrameImageAtIndex(m) with m < n, unless they first call clear(true).
    // This ensures that stateful ImageSources/decoders will work properly.
    //
    // The |data| and |allDataReceived| parameters should be supplied by callers
    // who set |destroyAll| to true if they wish to be able to continue using
    // the ImageSource.  This way implementations which choose to destroy their
    // decoders in some cases can reconstruct them correctly.
    void clear(bool destroyAll, size_t clearBeforeFrame = 0, SharedBuffer* data = nullptr, bool allDataReceived = false);

    // FIXME: Remove the decoder() function from this class when caching the ImageFrame is moved outside BitmapImage.
    ImageDecoder* decoder() const { return m_decoder.get(); }
    bool initialized() const { return m_decoder.get(); }

    void setData(SharedBuffer* data, bool allDataReceived);
    
    void setNeedsUpdateMetadata() { m_needsUpdateMetadata = true; }

    SubsamplingLevel subsamplingLevelForScale(float);
    void setAllowSubsampling(bool allowSubsampling) { m_allowSubsampling = allowSubsampling; }
    static size_t bytesDecodedToDetermineProperties();
    
    bool isSizeAvailable() const;
    // Always original size, without subsampling.
    IntSize size() const;
    IntSize sizeRespectingOrientation() const;

    size_t frameCount();
    RepetitionCount repetitionCount();
    String filenameExtension() const;
    Optional<IntPoint> hotSpot() const;

    bool frameIsCompleteAtIndex(size_t); // Whether or not the frame is completely decoded.
    bool frameHasAlphaAtIndex(size_t); // Whether or not the frame actually used any alpha.
    bool frameAllowSubsamplingAtIndex(size_t) const;
    
    // Size of optionally subsampled frame.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, RespectImageOrientationEnum = DoNotRespectImageOrientation) const;
    
    // Return the number of bytes in the decoded frame. If the frame is not yet
    // decoded then return 0.
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const;
    
    float frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t) const; // EXIF image orientation
    
    // Callers should not call this after calling clear() with a higher index;
    // see comments on clear() above.
    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    
private:
    void clearFrameBufferCache(size_t);
    SubsamplingLevel calculateMaximumSubsamplingLevel() const;
    void updateMetadata();
    void dump(TextStream&) const;
    
    std::unique_ptr<ImageDecoder> m_decoder;
    
    bool m_needsUpdateMetadata { false };
    size_t m_frameCount { 0 };
    Optional<SubsamplingLevel> m_maximumSubsamplingLevel { SubsamplingLevel::Default };

    // The default value of m_allowSubsampling should be the same as defaultImageSubsamplingEnabled in Settings.cpp
#if PLATFORM(IOS)
    bool m_allowSubsampling { true };
#else
    bool m_allowSubsampling { false };
#endif

    AlphaOption m_alphaOption { AlphaPremultiplied };
    GammaAndColorProfileOption m_gammaAndColorProfileOption { GammaAndColorProfileApplied };
};

}

#endif
