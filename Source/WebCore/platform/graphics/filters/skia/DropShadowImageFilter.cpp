/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "DropShadowImageFilter.h"

#include "SkBitmap.h"
#include "SkBlurImageFilter.h"
#include "SkCanvas.h"
#include "SkColorMatrixFilter.h"
#include "SkDevice.h"
#include "SkFlattenableBuffers.h"

namespace WebCore {

DropShadowImageFilter::DropShadowImageFilter(SkScalar dx, SkScalar dy, SkScalar sigma, SkColor color, SkImageFilter* input)
    : SkImageFilter(input)
    , m_dx(dx)
    , m_dy(dy)
    , m_sigma(sigma)
    , m_color(color)
{
}

DropShadowImageFilter::DropShadowImageFilter(SkFlattenableReadBuffer& buffer) : SkImageFilter(buffer)
{
    m_dx = buffer.readScalar();
    m_dy = buffer.readScalar();
    m_sigma = buffer.readScalar();
    m_color = buffer.readColor();
}

void DropShadowImageFilter::flatten(SkFlattenableWriteBuffer& buffer) const
{
    buffer.writeScalar(m_dx);
    buffer.writeScalar(m_dy);
    buffer.writeScalar(m_sigma);
    buffer.writeColor(m_color);
}

bool DropShadowImageFilter::onFilterImage(Proxy* proxy, const SkBitmap& source, const SkMatrix& matrix, SkBitmap* result, SkIPoint* loc)
{
    SkBitmap src = this->getInputResult(0, proxy, source, matrix, loc);
    SkAutoTUnref<SkDevice> device(proxy->createDevice(src.width(), src.height()));
    SkCanvas canvas(device.get());

    SkAutoTUnref<SkImageFilter> blurFilter(new SkBlurImageFilter(m_sigma, m_sigma));
    SkAutoTUnref<SkColorFilter> colorFilter(SkColorFilter::CreateModeFilter(m_color, SkXfermode::kSrcIn_Mode));
    SkPaint paint;
    paint.setImageFilter(blurFilter.get());
    paint.setColorFilter(colorFilter.get());
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    canvas.saveLayer(0, &paint);
    canvas.drawBitmap(src, m_dx, -m_dy);
    canvas.restore();
    canvas.drawBitmap(src, 0, 0);
    *result = device.get()->accessBitmap(false);
    return true;
}

};
