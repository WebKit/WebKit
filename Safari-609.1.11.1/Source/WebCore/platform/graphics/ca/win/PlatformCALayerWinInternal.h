/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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

#ifndef PlatformCALayerWinInternal_h
#define PlatformCALayerWinInternal_h

#include <CoreGraphics/CGGeometry.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

typedef struct _CACFLayer *CACFLayerRef;
typedef struct CGContext *CGContextRef;

namespace WebCore {

class Color;
class FloatRect;
class PlatformCALayer;
class TileController;
class TiledBacking;

typedef Vector<RefPtr<PlatformCALayer>> PlatformCALayerList;

class PlatformCALayerWinInternal {
public:
    PlatformCALayerWinInternal(PlatformCALayer*);
    ~PlatformCALayerWinInternal();

    virtual void displayCallback(CACFLayerRef, CGContextRef);
    virtual void setNeedsDisplayInRect(const FloatRect&);
    virtual void setNeedsDisplay();
    PlatformCALayer* owner() const { return m_owner; }

    void setSublayers(const PlatformCALayerList&);
    void getSublayers(PlatformCALayerList&) const;
    void removeAllSublayers();
    void insertSublayer(PlatformCALayer&, size_t);
    size_t sublayerCount() const;
    int indexOfSublayer(const PlatformCALayer* reference);

    virtual bool isOpaque() const;
    virtual void setOpaque(bool);

    virtual void setBounds(const FloatRect&);
    void setFrame(const FloatRect&);

    virtual float contentsScale() const;
    virtual void setContentsScale(float);

    virtual void setBorderWidth(float);

    virtual void setBorderColor(const Color&);

    void drawRepaintCounters(CACFLayerRef, CGContextRef, CGRect layerBounds, int drawCount);

private:
    void internalSetNeedsDisplay(const FloatRect*);
    PlatformCALayer* sublayerAtIndex(int) const;

    PlatformCALayer* m_owner;
};

}

#endif // PlatformCALayerWinInternal_h
