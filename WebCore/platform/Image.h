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

class QString;

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#if __OBJC__
@class NSImage;
#else
class NSImage;
#endif
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class IntRect;
class IntSize;
class QPainter;

template <typename T> class Array;
typedef Array<char> ByteArray;

// This class represents the platform-specific image data.
class ImageData;

class Image;

// This class gets notified when an image advances animation frames.
class ImageAnimationObserver;

class Image {
public:
    Image();
    Image(ImageAnimationObserver* observer, bool isPDF = false);
    ~Image();
    
    static Image* loadResource(const char *name);
    static bool supportsType(const QString& type);

    bool isNull() const;

    IntSize size() const;
    IntRect rect() const;
    int width() const;
    int height() const;

    bool setData(const ByteArray& bytes, bool allDataReceived);

    // It may look unusual that there are no start/stop animation calls as public API.  This is because
    // we start and stop animating lazily.  Animation begins whenever someone draws the image.  It will
    // automatically pause once all observers no longer want to render the image anywhere.
    void stopAnimation() const;
    void resetAnimation() const;
    
    // Typically the CachedImage that owns us.
    ImageAnimationObserver* animationObserver() const { return m_animationObserver; }
    
#if __APPLE__
    // Apple Image accessors for native formats.
    CGImageRef getCGImageRef() const;
    NSImage* getNSImage() const;
    CFDataRef getTIFFRepresentation() const;
#endif

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

    static CompositeOperator compositeOperatorFromString(const char* compositeOperator);

    // Drawing routines.
    void drawInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                    CompositeOperator compositeOp, void* context) const;
    void tileInRect(const FloatRect& dstRect, const FloatPoint& point, void* context) const;
    void scaleAndTileInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                            TileRule hRule, TileRule vRule, void* context) const;

private:
    // We do not allow images to be assigned to or copied.
    Image(const Image&);
    Image &operator=(const Image&);

    // The platform-specific image data representation.
    ImageData* m_data;

    // Our animation observer.
    ImageAnimationObserver* m_animationObserver;
};

}

#endif
