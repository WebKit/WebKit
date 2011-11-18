/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CrossfadeGeneratedImage_h
#define CrossfadeGeneratedImage_h

#include "CachedImage.h"
#include "GeneratedImage.h"
#include "Image.h"
#include "ImageObserver.h"
#include "IntSize.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSCrossfadeValue;
class CrossfadeSubimageObserverProxy;

class CrossfadeGeneratedImage : public GeneratedImage {
    friend class CrossfadeSubimageObserverProxy;
public:
    static PassRefPtr<CrossfadeGeneratedImage> create(CachedImage* fromImage, CachedImage* toImage, float percentage, ImageObserver* observer, IntSize crossfadeSize, const IntSize& size)
    {
        return adoptRef(new CrossfadeGeneratedImage(fromImage, toImage, percentage, observer, crossfadeSize, size));
    }
    virtual ~CrossfadeGeneratedImage();

protected:
    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace styleColorSpace, CompositeOperator);
    virtual void drawPattern(GraphicsContext*, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator, const FloatRect& destRect);

    CrossfadeGeneratedImage(CachedImage* fromImage, CachedImage* toImage, float percentage, ImageObserver*, IntSize crossfadeSize, const IntSize&);

    void imageChanged(CachedImage*, const IntRect* = 0);

private:
    void drawCrossfade(GraphicsContext*, const FloatRect& srcRect);

    // These are owned by the CSSCrossfadeValue that owns us.
    CachedImage* m_fromImage;
    CachedImage* m_toImage;

    float m_percentage;
    IntSize m_crossfadeSize;

    ImageObserver* m_observer;
    OwnPtr<CrossfadeSubimageObserverProxy> m_crossfadeSubimageObserver;
};

class CrossfadeSubimageObserverProxy : public CachedImageClient {
public:
    CrossfadeSubimageObserverProxy(CrossfadeGeneratedImage* ownerValue)
        : m_ownerValue(ownerValue)
        , m_ready(false) { }

    virtual ~CrossfadeSubimageObserverProxy() { }
    virtual void imageChanged(CachedImage*, const IntRect* = 0) OVERRIDE;
    void setReady(bool ready) { m_ready = ready; }
private:
    CrossfadeGeneratedImage* m_ownerValue;
    bool m_ready;
};

}

#endif
