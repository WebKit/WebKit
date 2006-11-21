/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *               2006 Rob Buis <buis@kde.org>
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
#ifdef SVG_SUPPORT

#include "KRenderingDevice.h"

typedef struct CGRect CGRect;
typedef struct CGContext *CGContextRef;

namespace WebCore {

class KRenderingDeviceContextQuartz : public KRenderingDeviceContext {
public:
    KRenderingDeviceContextQuartz(CGContextRef);
    virtual ~KRenderingDeviceContextQuartz();
    
    virtual AffineTransform concatCTM(const AffineTransform&);
    virtual AffineTransform ctm() const;
    
    virtual void clearPath();
    virtual void addPath(const Path&);

    virtual GraphicsContext* createGraphicsContext();
    
    CGContextRef cgContext() const { return m_cgContext; };
    
private:
    CGContextRef m_cgContext;
};

class KRenderingDeviceQuartz : public KRenderingDevice {
public:
    KRenderingDeviceQuartz() { }
    virtual ~KRenderingDeviceQuartz() { }

    virtual bool isBuffered() const { return false; }

    // context management.
    KRenderingDeviceContextQuartz* quartzContext() const;
    CGContextRef currentCGContext() const;
    virtual KRenderingDeviceContext* contextForImage(SVGResourceImage*) const;

    // Resource creation
    virtual PassRefPtr<SVGResource> createResource(const SVGResourceType&) const;
    virtual PassRefPtr<SVGPaintServer> createPaintServer(const SVGPaintServerType&) const;
    virtual SVGFilterEffect *createFilterEffect(const SVGFilterEffectType&) const;
    
    // filters (mostly debugging)
    static bool filtersEnabled();
    static void setFiltersEnabled(bool);
    static bool hardwareRenderingEnabled();
    static void setHardwareRenderingEnabled(bool);
};

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KRenderingDeviceQuartz_H
