/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved. 
 *               2006 Alexander Kellett <lypanov@kde.org>
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

#import <kcanvas/device/KRenderingDevice.h>
#import <kcanvas/device/KRenderingPaintServerSolid.h>
#import <kcanvas/device/KRenderingPaintServerPattern.h>
#import <kcanvas/device/KRenderingPaintServerGradient.h>

namespace WebCore {

class KCanvasImage;

class KRenderingPaintServerQuartzHelper {
public:
    static void strokePath(CGContextRef, const RenderPath *renderPath);
    static void clipToStrokePath(CGContextRef, const RenderPath *renderPath);
    static void fillPath(CGContextRef, const RenderPath *renderPath);
    static void clipToFillPath(CGContextRef, const RenderPath *renderPath);
};

class KRenderingPaintServerSolidQuartz : public KRenderingPaintServerSolid {
public:
    KRenderingPaintServerSolidQuartz() {};
    virtual void draw(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
    virtual bool setup(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
    virtual void teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
protected:
    virtual void renderPath(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
};

class KRenderingPaintServerPatternQuartz : public KRenderingPaintServerPattern {
public:
    KRenderingPaintServerPatternQuartz() {}
    virtual void draw(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
    virtual bool setup(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
    virtual void teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
protected:
    virtual void renderPath(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
private:
    mutable CGColorSpaceRef m_patternSpace;
    mutable CGPatternRef m_pattern;
};

typedef struct {
    CGFloat colorArray[4];
    CGFloat offset;
    CGFloat previousDeltaInverse;
} QuartzGradientStop;

// Does not subclass from KRenderingPaintServerGradient intentionally.
class KRenderingPaintServerGradientQuartz {

public:
    KRenderingPaintServerGradientQuartz();
    virtual ~KRenderingPaintServerGradientQuartz();
    
    void updateQuartzGradientStopsCache(const Vector<KCGradientStop>& stops);
    void updateQuartzGradientCache(const KRenderingPaintServerGradient* server);

    void draw(const KRenderingPaintServerGradient* server, KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
    bool setup(const KRenderingPaintServerGradient* server, KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
    void teardown(const KRenderingPaintServerGradient* server, KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
    void renderPath(const KRenderingPaintServerGradient* server, KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;

public:
    // due to c functions
    QuartzGradientStop *m_stopsCache;
    int                 m_stopsCount;

protected:
    void invalidateCaches();
    CGShadingRef m_shadingCache;
    mutable KCanvasImage *m_maskImage;
};

class KRenderingPaintServerLinearGradientQuartz : public KRenderingPaintServerGradientQuartz,
                                                  public KRenderingPaintServerLinearGradient {
public:
    KRenderingPaintServerLinearGradientQuartz() { }
    virtual void invalidate();
    virtual void draw(KRenderingDeviceContext* context, const RenderPath* renderPath, KCPaintTargetType type) const
        { KRenderingPaintServerGradientQuartz::draw(this, context, renderPath, type); }
    virtual bool setup(KRenderingDeviceContext* context, const RenderObject* renderObject, KCPaintTargetType type) const
        { return KRenderingPaintServerGradientQuartz::setup(this, context, renderObject, type); }
    virtual void teardown(KRenderingDeviceContext* context, const RenderObject* renderObject, KCPaintTargetType type) const
        { KRenderingPaintServerGradientQuartz::teardown(this, context, renderObject, type); }
    virtual void renderPath(KRenderingDeviceContext* context, const RenderPath* renderPath, KCPaintTargetType type) const
        { KRenderingPaintServerGradientQuartz::renderPath(this, context, renderPath, type); }
};

class KRenderingPaintServerRadialGradientQuartz : public KRenderingPaintServerGradientQuartz,
                                                  public KRenderingPaintServerRadialGradient {
public:
    KRenderingPaintServerRadialGradientQuartz() { }
    virtual void invalidate();
    virtual void draw(KRenderingDeviceContext* context, const RenderPath* renderPath, KCPaintTargetType type) const
        { KRenderingPaintServerGradientQuartz::draw(this, context, renderPath, type); }
    virtual bool setup(KRenderingDeviceContext* context, const RenderObject* renderObject, KCPaintTargetType type) const
        { return KRenderingPaintServerGradientQuartz::setup(this, context, renderObject, type); }
    virtual void teardown(KRenderingDeviceContext* context, const RenderObject* renderObject, KCPaintTargetType type) const
        { KRenderingPaintServerGradientQuartz::teardown(this, context, renderObject, type); }
    virtual void renderPath(KRenderingDeviceContext* context, const RenderPath* renderPath, KCPaintTargetType type) const
        { KRenderingPaintServerGradientQuartz::renderPath(this, context, renderPath, type); }
};

}
