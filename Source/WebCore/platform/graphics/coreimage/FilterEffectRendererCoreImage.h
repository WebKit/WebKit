/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if USE(CORE_IMAGE)

#import "FilterEffectRenderer.h"
#import "PlatformImageBuffer.h"
#import <wtf/Vector.h>

OBJC_CLASS CIImage;
OBJC_CLASS CIFilter;
OBJC_CLASS CIContext;

namespace WebCore {

class FEColorMatrix;
class FEComponentTransfer;
class SourceGraphic;

class FilterEffectRendererCoreImage : public FilterEffectRenderer {
    WTF_MAKE_FAST_ALLOCATED;
    
public:
    static std::unique_ptr<FilterEffectRendererCoreImage> tryCreate(FilterEffect&);
    RetainPtr<CIContext> sharedCIContext();
    void applyEffects(FilterEffect&) final;
    bool hasResult() const final { return m_outputImage; }
    ImageBuffer* output() const final;
    FloatRect destRect(const FilterEffect&) const final;
    void clearResult() final;
    FilterEffectRendererCoreImage();
    
private:
    RetainPtr<CIImage> connectCIFilters(FilterEffect&);
    void renderToImageBuffer(FilterEffect&) final;
    static bool supportsCoreImageRendering(FilterEffect&);
    static bool canRenderUsingCIFilters(FilterEffect&);
    
    RetainPtr<CIImage> imageForSourceGraphic(SourceGraphic&);
    RetainPtr<CIImage> imageForFEColorMatrix(const FEColorMatrix&, const Vector<RetainPtr<CIImage>>&);
    RetainPtr<CIImage> imageForFEComponentTransfer(const FEComponentTransfer&, Vector<RetainPtr<CIImage>>&);
    
    RefPtr<IOSurfaceImageBuffer> m_outputImageBuffer;
    RetainPtr<CIImage> m_outputImage;
};

} // namespace WebCore

#endif
