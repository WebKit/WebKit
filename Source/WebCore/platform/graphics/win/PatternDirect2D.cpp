/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "Pattern.h"

#if USE(DIRECT2D)

#include "AffineTransform.h"
#include "GraphicsContext.h"
#include "PlatformContextDirect2D.h"
#include <d2d1.h>
#include <wtf/MainThread.h>


namespace WebCore {

ID2D1BitmapBrush* Pattern::createPlatformPattern(const GraphicsContext& context, float alpha, const AffineTransform& userSpaceTransformation) const
{
    AffineTransform patternTransform = userSpaceTransformation * m_patternSpaceTransformation;
    auto bitmapBrushProperties = D2D1::BitmapBrushProperties();
    bitmapBrushProperties.extendModeX = m_repeatX ? D2D1_EXTEND_MODE_WRAP : D2D1_EXTEND_MODE_CLAMP;
    bitmapBrushProperties.extendModeY = m_repeatY ? D2D1_EXTEND_MODE_WRAP : D2D1_EXTEND_MODE_CLAMP;

    auto brushProperties = D2D1::BrushProperties();
    brushProperties.transform = patternTransform;
    brushProperties.opacity = alpha;

    auto& patternImage = tileImage();
    auto nativeImage = patternImage.nativeImage(&context);
    if (!nativeImage)
        return nullptr;

    auto platformContext = context.platformContext();
    RELEASE_ASSERT(platformContext);

    ID2D1BitmapBrush* patternBrush = nullptr;
    HRESULT hr = platformContext->renderTarget()->CreateBitmapBrush(nativeImage.get(), &bitmapBrushProperties, &brushProperties, &patternBrush);
    ASSERT(SUCCEEDED(hr));
    return patternBrush;
}

}

#endif
