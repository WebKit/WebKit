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
#if USE(SKIA)
#include "FEComposite.h"

#include "SkFlattenableBuffers.h"
#include "SkMorphologyImageFilter.h"
#include "SkiaImageFilterBuilder.h"

namespace {

class CompositeImageFilter : public SkImageFilter {
public:
    CompositeImageFilter(SkXfermode::Mode mode, SkImageFilter* background, SkImageFilter* foreground) : SkImageFilter(background, foreground), m_mode(mode)
    {
    }

    virtual bool onFilterImage(Proxy* proxy, const SkBitmap& src, const SkMatrix& ctm, SkBitmap* dst, SkIPoint* offset)
    {
        SkBitmap background, foreground = src;
        SkImageFilter* backgroundInput = getInput(0);
        SkImageFilter* foregroundInput = getInput(1);
        SkIPoint backgroundOffset = SkIPoint::Make(0, 0), foregroundOffset = SkIPoint::Make(0, 0);
        if (backgroundInput && !backgroundInput->filterImage(proxy, src, ctm, &background, &backgroundOffset))
            return false;

        if (foregroundInput && !foregroundInput->filterImage(proxy, src, ctm, &foreground, &foregroundOffset))
            return false;

        dst->setConfig(background.config(), background.width(), background.height());
        dst->allocPixels();
        SkCanvas canvas(*dst);
        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        canvas.drawBitmap(background, backgroundOffset.fX, backgroundOffset.fY, &paint);
        paint.setXfermodeMode(m_mode);
        canvas.drawBitmap(foreground, foregroundOffset.fX, foregroundOffset.fY, &paint);
        return true;
    }

    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(CompositeImageFilter)

protected:
    explicit CompositeImageFilter(SkFlattenableReadBuffer& buffer)
        : SkImageFilter(buffer)
    {
        m_mode = (SkXfermode::Mode) buffer.readInt();
    }

    virtual void flatten(SkFlattenableWriteBuffer& buffer) const
    {
        this->SkImageFilter::flatten(buffer);
        buffer.writeInt((int) m_mode);
    }

private:
    SkXfermode::Mode m_mode;
};

SkXfermode::Mode toXfermode(WebCore::CompositeOperationType mode)
{
    switch (mode) {
    case WebCore::FECOMPOSITE_OPERATOR_OVER:
        return SkXfermode::kSrcOver_Mode;
    case WebCore::FECOMPOSITE_OPERATOR_IN:
        return SkXfermode::kSrcIn_Mode;
    case WebCore::FECOMPOSITE_OPERATOR_OUT:
        return SkXfermode::kDstOut_Mode;
    case WebCore::FECOMPOSITE_OPERATOR_ATOP:
        return SkXfermode::kSrcATop_Mode;
    case WebCore::FECOMPOSITE_OPERATOR_XOR:
        return SkXfermode::kXor_Mode;
    default:
        ASSERT_NOT_REACHED();
        return SkXfermode::kSrcOver_Mode;
    }
}

};

namespace WebCore {

SkImageFilter* FEComposite::createImageFilter(SkiaImageFilterBuilder* builder)
{
    SkAutoTUnref<SkImageFilter> foreground(builder->build(inputEffect(0)));
    SkAutoTUnref<SkImageFilter> background(builder->build(inputEffect(1)));
    if (m_type == FECOMPOSITE_OPERATOR_ARITHMETIC)
        return 0; // FIXME: Implement arithmetic op
    return new CompositeImageFilter(toXfermode(m_type), foreground, background);
}

};
#endif
