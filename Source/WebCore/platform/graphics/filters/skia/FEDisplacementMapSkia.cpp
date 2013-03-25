/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(FILTERS)
#include "FEDisplacementMap.h"

#include "NativeImageSkia.h"
#include "SkBitmapSource.h"
#include "SkDisplacementMapEffect.h"
#include "SkiaImageFilterBuilder.h"

namespace WebCore {

static SkDisplacementMapEffect::ChannelSelectorType toSkiaMode(ChannelSelectorType type)
{
    switch (type) {
    case CHANNEL_R:
        return SkDisplacementMapEffect::kR_ChannelSelectorType;
    case CHANNEL_G:
        return SkDisplacementMapEffect::kG_ChannelSelectorType;
    case CHANNEL_B:
        return SkDisplacementMapEffect::kB_ChannelSelectorType;
    case CHANNEL_A:
        return SkDisplacementMapEffect::kA_ChannelSelectorType;
    case CHANNEL_UNKNOWN:
    default:
        return SkDisplacementMapEffect::kUnknown_ChannelSelectorType;
    }
}

bool FEDisplacementMap::platformApplySkia()
{
    // For now, only use the skia implementation for accelerated rendering.
    if (filter()->renderingMode() != Accelerated)
        return false;

    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);

    if (!in || !in2) 
        return false;

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    RefPtr<Image> color = in->asImageBuffer()->copyImage(DontCopyBackingStore);
    RefPtr<Image> displ = in2->asImageBuffer()->copyImage(DontCopyBackingStore);

    NativeImageSkia* colorNativeImage = color->nativeImageForCurrentFrame();
    NativeImageSkia* displNativeImage = displ->nativeImageForCurrentFrame();

    if (!colorNativeImage || !displNativeImage)
        return false;

    SkBitmap colorBitmap = colorNativeImage->bitmap();
    SkBitmap displBitmap = displNativeImage->bitmap();

    SkAutoTUnref<SkImageFilter> colorSource(new SkBitmapSource(colorBitmap));
    SkAutoTUnref<SkImageFilter> displSource(new SkBitmapSource(displBitmap));
    SkDisplacementMapEffect::ChannelSelectorType typeX = toSkiaMode(m_xChannelSelector);
    SkDisplacementMapEffect::ChannelSelectorType typeY = toSkiaMode(m_yChannelSelector);
    SkAutoTUnref<SkImageFilter> displEffect(new SkDisplacementMapEffect(
        typeX, typeY, SkFloatToScalar(m_scale), displSource, colorSource));
    SkPaint paint;
    paint.setImageFilter(displEffect);
    resultImage->context()->platformContext()->drawBitmap(colorBitmap, 0, 0, &paint);
    return true;
}

SkImageFilter* FEDisplacementMap::createImageFilter(SkiaImageFilterBuilder* builder)
{
    SkImageFilter* color = builder->build(inputEffect(0));
    SkImageFilter* displ = builder->build(inputEffect(1));
    SkDisplacementMapEffect::ChannelSelectorType typeX = toSkiaMode(m_xChannelSelector);
    SkDisplacementMapEffect::ChannelSelectorType typeY = toSkiaMode(m_yChannelSelector);
    return new SkDisplacementMapEffect(typeX, typeY, SkFloatToScalar(m_scale), displ, color);
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
