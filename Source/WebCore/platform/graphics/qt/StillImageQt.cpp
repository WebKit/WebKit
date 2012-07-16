/*
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * All rights reserved.
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

#include "config.h"
#include "StillImageQt.h"

#include "GraphicsContext.h"
#include "IntSize.h"
#include "ShadowBlur.h"

#include <QPainter>

namespace WebCore {

StillImage::StillImage(const QImage& image)
    : m_image(new QImage(image))
    , m_ownsImage(true)
{}

StillImage::StillImage(const QImage* image)
    : m_image(image)
    , m_ownsImage(false)
{}

StillImage::~StillImage()
{
    if (m_ownsImage)
        delete m_image;
}

bool StillImage::currentFrameHasAlpha()
{
    return m_image->hasAlphaChannel();
}

IntSize StillImage::size() const
{
    return IntSize(m_image->width(), m_image->height());
}

NativeImagePtr StillImage::nativeImageForCurrentFrame()
{
    return const_cast<NativeImagePtr>(m_image);
}

void StillImage::draw(GraphicsContext* ctxt, const FloatRect& dst,
                      const FloatRect& src, ColorSpace, CompositeOperator op)
{
    if (m_image->isNull())
        return;

    FloatRect normalizedSrc = src.normalized();
    FloatRect normalizedDst = dst.normalized();

    CompositeOperator previousOperator = ctxt->compositeOperation();
    ctxt->setCompositeOperation(op);

    if (ctxt->hasShadow()) {
        ShadowBlur* shadow = ctxt->shadowBlur();
        GraphicsContext* shadowContext = shadow->beginShadowLayer(ctxt, normalizedDst);
        if (shadowContext) {
            QPainter* shadowPainter = shadowContext->platformContext();
            shadowPainter->drawImage(normalizedDst, *m_image, normalizedSrc);
            shadow->endShadowLayer(ctxt);
        }
    }

    ctxt->platformContext()->drawImage(normalizedDst, *m_image, normalizedSrc);
    ctxt->setCompositeOperation(previousOperator);
}

}
