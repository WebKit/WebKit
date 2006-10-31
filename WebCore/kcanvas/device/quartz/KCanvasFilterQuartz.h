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

#ifndef KCancasFilterQuartz_H
#define KCanvasFilterQuartz_H

#ifdef SVG_SUPPORT

#include "KCanvasFilters.h"

#ifdef __OBJC__
@class CIFilter;
@class CIImage;
#else
class CIFilter;
class CIImage;
class NSArray;
class CIContext;
class NSMutableDictionary;
#endif

namespace WebCore {

class KRenderingDevice;

class KCanvasFilterQuartz : public KCanvasFilter {
public:
    KCanvasFilterQuartz();
    virtual ~KCanvasFilterQuartz();
    
    virtual void prepareFilter(const FloatRect& bbox);
    virtual void applyFilter(const FloatRect& bbox);
 
    CIImage* imageForName(const String&) const;
    void setImageForName(CIImage*, const String&);
    
    void setOutputImage(const KCanvasFilterEffect*, CIImage*);
    CIImage* inputImage(const KCanvasFilterEffect*);

private:
#if PLATFORM(CI)
    NSArray* getCIFilterStack(CIImage* inputImage);

    CIContext* m_filterCIContext;
    CGLayerRef m_filterCGLayer;
    NSMutableDictionary* m_imagesByName;
#endif
};

class KCanvasFEBlendQuartz : public KCanvasFEBlend {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEColorMatrixQuartz : public KCanvasFEColorMatrix {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEComponentTransferQuartz : public KCanvasFEComponentTransfer {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
    
private:
    CIFilter* getFunctionFilter(KCChannelSelectorType, CIImage* inputImage) const;
#endif
};

class KCanvasFEConvolveMatrixQuartz : public KCanvasFEConvolveMatrix {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFECompositeQuartz : public KCanvasFEComposite {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEDiffuseLightingQuartz : public KCanvasFEDiffuseLighting {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEFloodQuartz : public KCanvasFEFlood {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEGaussianBlurQuartz : public KCanvasFEGaussianBlur {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEImageQuartz : public KCanvasFEImage {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEDisplacementMapQuartz : public KCanvasFEDisplacementMap {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEMergeQuartz : public KCanvasFEMerge {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFEOffsetQuartz : public KCanvasFEOffset {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFESpecularLightingQuartz : public KCanvasFESpecularLighting {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

class KCanvasFETileQuartz : public KCanvasFETile {
public:
#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(KCanvasFilterQuartz*) const;
#endif
};

}

#endif // SVG_SUPPORT
#endif // !KCanvasFilterQuartz_H
