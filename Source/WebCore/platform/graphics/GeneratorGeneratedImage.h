/*
 * Copyright (C) 2008, 2012 Apple Computer, Inc.  All rights reserved.
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

#ifndef GeneratorGeneratedImage_h
#define GeneratorGeneratedImage_h

#include "GeneratedImage.h"
#include "Generator.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "Timer.h"
#include <wtf/RefPtr.h>

namespace WebCore {

static const int generatedImageCacheClearDelay = 1;

class GeneratedImageCacheTimer;

class GeneratorGeneratedImage : public GeneratedImage {
    friend class GeneratedImageCacheTimer;
public:
    static PassRefPtr<GeneratorGeneratedImage> create(PassRefPtr<Generator> generator, const IntSize& size)
    {
        return adoptRef(new GeneratorGeneratedImage(generator, size));
    }

    virtual ~GeneratorGeneratedImage()
    {
        m_cacheTimer.stop();
    }

protected:
    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace styleColorSpace, CompositeOperator);
    virtual void drawPattern(GraphicsContext*, const FloatRect& srcRect, const AffineTransform& patternTransform,
                             const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator, const FloatRect& destRect);

    GeneratorGeneratedImage(PassRefPtr<Generator> generator, const IntSize& size)
        : m_generator(generator)
        , m_cacheTimer(this)
    {
        m_size = size;
    }
    
    class GeneratedImageCacheTimer : public TimerBase {
    public:
        GeneratedImageCacheTimer(GeneratorGeneratedImage * parent)
        : m_shouldRestartWhenTimerFires(false)
        , m_parent(parent) { }
        
        void restart()
        {
            if (isActive()) {
                m_shouldRestartWhenTimerFires = true;
                return;
            }
            startOneShot(generatedImageCacheClearDelay);
        };
    private:
        virtual void fired() OVERRIDE
        {
            if (m_shouldRestartWhenTimerFires) {
                m_shouldRestartWhenTimerFires = false;
                startOneShot(generatedImageCacheClearDelay);
                return;
            }
            
            if (m_parent) {
                m_parent->m_cachedImageBuffer.clear();
                m_parent->m_cachedAdjustedSize = IntSize();
            }
        };
        bool m_shouldRestartWhenTimerFires;
        GeneratorGeneratedImage* m_parent;
    };

    RefPtr<Generator> m_generator;

    OwnPtr<ImageBuffer> m_cachedImageBuffer;
    GeneratedImageCacheTimer m_cacheTimer;
    IntSize m_cachedAdjustedSize;
    unsigned m_cachedGeneratorHash;
};

}

#endif
