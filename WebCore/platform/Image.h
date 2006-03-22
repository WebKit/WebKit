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

#ifndef IMAGE_H_
#define IMAGE_H_

#include <kxmlcore/Vector.h>
#include "ImageSource.h"
#include "DeprecatedArray.h"
#include "IntSize.h"

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#if __OBJC__
@class NSImage;
#else
class NSImage;
#endif
#endif

namespace WebCore {
    struct FrameData;
};

// This complicated-looking declaration tells the framedata Vector that it can copy without
// having to invoke our copy constructor. This allows us to not have to worry about ref counting
// the native frames.
namespace KXMLCore { 
    template<> class VectorTraits<WebCore::FrameData> : public SimpleClassVectorTraits {};
}

namespace WebCore {

class FloatPoint;
class FloatRect;
class IntRect;
class IntSize;
class String;

template <typename T> class Timer;

// FIXME: Eventually we will want to have CoreGraphics vs. Cairo ifdefs in this file rather
// than Apple vs. non-Apple.

// This pointer represents the platform-specific image data.
#if __APPLE__
typedef CGImageRef NativeImagePtr;
class PDFDocumentImage;
typedef CFDataRef NativeBytePtr;
#else
typedef cairo_surface_t* NativeImagePtr;
typedef const DeprecatedByteArray* NativeBytePtr;
#endif

// This class gets notified when an image advances animation frames.
class ImageAnimationObserver;

// ================================================
// FrameData Class
// ================================================

struct FrameData {
    FrameData()
      :m_frame(0), m_duration(0), m_hasAlpha(true) 
    {}

    ~FrameData()
    { 
        clear();
    }

    void clear();

    NativeImagePtr m_frame;
    float m_duration;
    bool m_hasAlpha;
};

// =================================================
// Image Class
// =================================================

class Image {
public:
    Image();
    Image(ImageAnimationObserver* observer, bool isPDF = false);
    ~Image();
    
    static Image* loadResource(const char *name);
    static bool supportsType(const String& type);

    bool isNull() const;

    IntSize size() const;
    IntRect rect() const;
    int width() const;
    int height() const;

    bool setData(const DeprecatedByteArray& bytes, bool allDataReceived);
    bool setNativeData(NativeBytePtr bytePtr, bool allDataReceived);

    // It may look unusual that there is no start animation call as public API.  This is because
    // we start and stop animating lazily.  Animation begins whenever someone draws the image.  It will
    // automatically pause once all observers no longer want to render the image anywhere.
    void stopAnimation();
    void resetAnimation();
    
    // Frame accessors.
    size_t currentFrame() const { return m_currentFrame; }
    size_t frameCount();
    NativeImagePtr frameAtIndex(size_t index);
    float frameDurationAtIndex(size_t index);
    bool frameHasAlphaAtIndex(size_t index);

    // Typically the CachedImage that owns us.
    ImageAnimationObserver* animationObserver() const { return m_animationObserver; }

    // Note: These constants exactly match the NSCompositeOperator constants of AppKit.
    enum CompositeOperator {
        CompositeClear,
        CompositeCopy,
        CompositeSourceOver,
        CompositeSourceIn,
        CompositeSourceOut,
        CompositeSourceAtop,
        CompositeDestinationOver,
        CompositeDestinationIn,
        CompositeDestinationOut,
        CompositeDestinationAtop,
        CompositeXOR,
        CompositePlusDarker,
        CompositeHighlight,
        CompositePlusLighter
    };

    enum TileRule { StretchTile, RoundTile, RepeatTile };

    static CompositeOperator compositeOperatorFromString(const String&);

    // Drawing routines.
    void drawInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                    CompositeOperator compositeOp, void* context);
    void tileInRect(const FloatRect& dstRect, const FloatPoint& point, void* context);
    void scaleAndTileInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                            TileRule hRule, TileRule vRule, void* context);

#if __APPLE__
    // Apple Image accessors for native formats.
    CGImageRef getCGImageRef();
    NSImage* getNSImage();
    CFDataRef getTIFFRepresentation();
    
    // PDF
    void setIsPDF() { m_isPDF = true; }
    
private:
    void checkForSolidColor(CGImageRef image);
#endif

private:
    // We do not allow images to be assigned to or copied.
    Image(const Image&);
    Image &operator=(const Image&);

    // Decodes and caches a frame. Never accessed except internally.
    void cacheFrame(size_t index);

    // Called to invalidate all our cached data when more bytes are available.
    void invalidateData();

    // Whether or not size is available yet.    
    bool isSizeAvailable();

    // Animation.
    bool shouldAnimate();
    void startAnimation();
    void advanceAnimation(Timer<Image>* timer);
    
    // Constructor for native data.
    void initNativeData();

    // Destructor for native data.
    void destroyNativeData();

    // Invalidation of native data.
    void invalidateNativeData();

    // Members
    DeprecatedByteArray m_data; // The encoded raw data for the image.
    ImageSource m_source;
    mutable IntSize m_size; // The size to use for the overall image (will just be the size of the first image).
    
    size_t m_currentFrame; // The index of the current frame of animation.
    Vector<FrameData> m_frames; // An array of the cached frames of the animation. We have to ref frames to pin them in the cache.
    
    // Our animation observer.
    ImageAnimationObserver* m_animationObserver;
    Timer<Image>* m_frameTimer;
    int m_repetitionCount; // How many total animation loops we should do.
    int m_repetitionsComplete;  // How many repetitions we've finished.

#if __APPLE__
    mutable NSImage* m_nsImage; // A cached NSImage of frame 0. Only built lazily if someone actually queries for one.
    mutable CFDataRef m_tiffRep; // Cached TIFF rep for frame 0.  Only built lazily if someone queries for one.
    PDFDocumentImage* m_PDFDoc;
    CGColorRef m_solidColor; // Will be 0 if transparent.
    bool m_isSolidColor : 1; // If the image is 1x1 with no alpha, we can just do a rect fill when painting/tiling.
    bool m_isPDF : 1;
#endif

    bool m_animatingImageType : 1;  // Whether or not we're an image type that is capable of animating (GIF).
    bool m_animationFinished : 1;  // Whether or not we've completed the entire animation.

    mutable bool m_haveSize : 1; // Whether or not our |m_size| member variable has the final overall image size yet.
    bool m_sizeAvailable : 1; // Whether or not we can obtain the size of the first image frame yet from ImageIO.
};

}

#endif
