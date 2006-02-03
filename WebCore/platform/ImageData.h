/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef IMAGE_DATA_H_
#define IMAGE_DATA_H_

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#if __OBJC__
@class NSImage;
#else
class NSImage;
#endif
#endif

namespace WebCore {
    class FrameData;
}

// This complicated-looking declaration tells the framedata Vector that it can copy without
// having to invoke our copy constructor. This allows us to not have to worry about ref counting
// the native frames.
namespace KXMLCore { 
    template<> class VectorTraits<WebCore::FrameData> : public SimpleClassVectorTraits {};
}

namespace WebCore {

class Image;

template <typename T> class Array;
typedef Array<char> ByteArray;

template <typename T> class Timer;

#if __APPLE__
typedef CGImageRef NativeImagePtr;
class PDFDocumentImage;
typedef CFDataRef NativeBytePtr;
#else
typedef void* NativeImagePtr;
typedef ByteArray* NativeBytePtr;
#endif

// ================================================
// FrameData Class
// ================================================

struct FrameData {
    FrameData()
      :m_frame(0), m_duration(0) 
    {}

    ~FrameData()
    { 
        clear();
    }

    void clear();

    NativeImagePtr m_frame;
    float m_duration;
};

// ================================================
// ImageData Class
// ================================================

class ImageData {
public:
    ImageData(Image*);
    ~ImageData();

    bool isNull() const;
    IntSize size() const;

#if __APPLE__
    void setIsPDF() { m_isPDF = true; }
#endif

    // |bytes| contains all of the data that we have seen so far.
    bool setData(const ByteArray& bytes, bool allDataReceived);
    bool setNativeData(NativeBytePtr bytePtr, bool allDataReceived);

    size_t currentFrame() const { return m_currentFrame; }
    size_t frameCount() const;
    NativeImagePtr frameAtIndex(size_t index);
#if __APPLE__
    NSImage* getNSImage();
    CFDataRef getTIFFRepresentation();
    void invalidateAppleSpecificData();
#endif

    float frameDurationAtIndex(size_t index);
    
    bool shouldAnimate() const;
    void startAnimation();
    void stopAnimation();
    void resetAnimation();
    void advanceAnimation(Timer<ImageData>* timer);

private:
    bool isSizeAvailable();
    
    void invalidateData();
    void cacheFrame(size_t index);

#if __APPLE__
    void checkForSolidColor(CGImageRef image);
#endif

    ByteArray m_data; // The encoded raw data for the image.
    Image* m_image;
    ImageDecoder m_decoder;
    mutable IntSize m_size; // The size to use for the overall image (will just be the size of the first image).
    
    size_t m_currentFrame; // The index of the current frame of animation.
    Vector<FrameData> m_frames; // An array of the cached frames of the animation. We have to ref frames to pin them in the cache.
#if __APPLE__
    NSImage* m_nsImage; // A cached NSImage of frame 0. Only built lazily if someone actually queries for one.
    CFDataRef m_tiffRep; // Cached TIFF rep for frame 0.  Only built lazily if someone queries for one.
#endif

    Timer<ImageData>* m_frameTimer;
    int m_repetitionCount; // How many total animation loops we should do.
    int m_repetitionsComplete;  // How many repetitions we've finished.

#if __APPLE__
    CGColorRef m_solidColor; // Will be 0 if transparent.
    bool m_isSolidColor : 1; // If the image is 1x1 with no alpha, we can just do a rect fill when painting/tiling.
#endif

    bool m_animatingImageType : 1;  // Whether or not we're an image type that is capable of animating (GIF).
    bool m_animationFinished : 1;  // Whether or not we've completed the entire animation.

    mutable bool m_haveSize : 1; // Whether or not our |m_size| member variable has the final overall image size yet.
    bool m_sizeAvailable : 1; // Whether or not we can obtain the size of the first image frame yet from ImageIO.
#if __APPLE__
    bool m_isPDF : 1;
    PDFDocumentImage* m_PDFDoc;
#endif

    friend class Image;
};

}

#endif
