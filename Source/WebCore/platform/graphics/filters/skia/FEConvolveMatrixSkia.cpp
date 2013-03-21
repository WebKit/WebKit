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
#include "FEConvolveMatrix.h"

#include "SkMatrixConvolutionImageFilter.h"
#include "SkiaImageFilterBuilder.h"
#include <wtf/OwnArrayPtr.h>

namespace {

SkMatrixConvolutionImageFilter::TileMode toSkiaTileMode(WebCore::EdgeModeType edgeMode)
{
    switch (edgeMode) {
    case WebCore::EDGEMODE_UNKNOWN:
    case WebCore::EDGEMODE_DUPLICATE:
        return SkMatrixConvolutionImageFilter::kClamp_TileMode;
    case WebCore::EDGEMODE_WRAP:
        return SkMatrixConvolutionImageFilter::kRepeat_TileMode;
    case WebCore::EDGEMODE_NONE:
        return SkMatrixConvolutionImageFilter::kClampToBlack_TileMode;
    }
}

}; // unnamed namespace

namespace WebCore {

SkImageFilter* FEConvolveMatrix::createImageFilter(SkiaImageFilterBuilder* builder)
{
    SkAutoTUnref<SkImageFilter> input(builder->build(inputEffect(0)));

    SkISize kernelSize(SkISize::Make(m_kernelSize.width(), m_kernelSize.height()));
    int numElements = kernelSize.width() * kernelSize.height();
    SkScalar gain = SkFloatToScalar(1.0f / m_divisor);
    SkScalar bias = SkFloatToScalar(m_bias);
    SkIPoint target = SkIPoint::Make(m_targetOffset.x(), m_targetOffset.y());
    SkMatrixConvolutionImageFilter::TileMode tileMode = toSkiaTileMode(m_edgeMode);
    bool convolveAlpha = !m_preserveAlpha;
    OwnArrayPtr<SkScalar> kernel = adoptArrayPtr(new SkScalar[numElements]);
    for (int i = 0; i < numElements; ++i)
        kernel[i] = SkFloatToScalar(m_kernelMatrix[numElements - 1 - i]);
    return new SkMatrixConvolutionImageFilter(kernelSize, kernel.get(), gain, bias, target, tileMode, convolveAlpha, input);
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
