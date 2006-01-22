/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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


#ifndef KRenderingDeviceQuartz_H
#define KRenderingDeviceQuartz_H
#if SVG_SUPPORT

#import <kcanvas/device/KRenderingDevice.h>

typedef struct CGRect CGRect;
typedef struct CGContext *CGContextRef;

#if __OBJC__
@class NSGraphicsContext;
#else
class NSGraphicsContext;
#endif // SVG_SUPPORT
#endif

class KRenderingDeviceContextQuartz : public KRenderingDeviceContext
{
public:
    KRenderingDeviceContextQuartz(CGContextRef context);
    virtual ~KRenderingDeviceContextQuartz();
    
    virtual KCanvasMatrix concatCTM(const KCanvasMatrix &worldMatrix);
    virtual KCanvasMatrix ctm() const;
    
    virtual IntRect mapFromVisual(const IntRect &rect);
    virtual IntRect mapToVisual(const IntRect &rect);
    
    virtual void clearPath();
    virtual void addPath(const KCanvasPath*);
    
    CGContextRef cgContext() const { return m_cgContext; };
    NSGraphicsContext *nsGraphicsContext();
    
private:
    CGContextRef m_cgContext;
    NSGraphicsContext *m_nsGraphicsContext;
};

class KRenderingDeviceQuartz : public KRenderingDevice
{
public:
    KRenderingDeviceQuartz() { }
    virtual ~KRenderingDeviceQuartz() { }

    virtual bool isBuffered() const { return false; }
    
    virtual KRenderingDeviceContext *popContext();
    virtual void pushContext(KRenderingDeviceContext *context);

    // context management.
    KRenderingDeviceContextQuartz *quartzContext() const;
    CGContextRef currentCGContext() const;
    virtual KRenderingDeviceContext *contextForImage(KCanvasImage *) const;

    virtual QString stringForPath(const KCanvasPath* path);

    // Resource creation
    virtual KCanvasResource *createResource(const KCResourceType &type) const;
    virtual KRenderingPaintServer *createPaintServer(const KCPaintServerType &type) const;
    virtual KCanvasFilterEffect *createFilterEffect(const KCFilterEffectType &type) const;
    virtual KCanvasPath* createPath() const;
    
    // item creation
    virtual RenderPath *createItem(RenderArena *arena, khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node, KCanvasPath* path) const;
    virtual KCanvasContainer *createContainer(RenderArena *arena, khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) const;

    // filters (mostly debugging)
    static bool filtersEnabled();
    static void setFiltersEnabled(bool enabled);
    static bool KRenderingDeviceQuartz::hardwareRenderingEnabled();
    static void KRenderingDeviceQuartz::setHardwareRenderingEnabled(bool enabled);
};

#endif
