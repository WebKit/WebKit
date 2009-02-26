/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG)
#include "SVGResourceMasker.h"

#include "CanvasPixelArray.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "GraphicsContext.h"
#include "SVGMaskElement.h"
#include "SVGRenderSupport.h"
#include "SVGRenderStyle.h"
#include "TextStream.h"

#include <wtf/ByteArray.h>

using namespace std;

namespace WebCore {

SVGResourceMasker::SVGResourceMasker(const SVGMaskElement* ownerElement)
    : SVGResource()
    , m_ownerElement(ownerElement)
{
}

SVGResourceMasker::~SVGResourceMasker()
{
}

void SVGResourceMasker::invalidate()
{
    SVGResource::invalidate();
    m_mask.clear();
}

void SVGResourceMasker::applyMask(GraphicsContext* context, const FloatRect& boundingBox)
{
    if (!m_mask)
        m_mask.set(m_ownerElement->drawMaskerContent(boundingBox, m_maskRect).release());

    if (!m_mask)
        return;

    IntSize imageSize(m_mask->size());
    IntRect intImageRect(0, 0, imageSize.width(), imageSize.height());

    // Create new ImageBuffer to apply luminance
    auto_ptr<ImageBuffer> luminancedImage(ImageBuffer::create(imageSize, false));
    if (!luminancedImage.get())
        return;

    PassRefPtr<CanvasPixelArray> srcPixelArray(m_mask->getImageData(intImageRect)->data());
    PassRefPtr<ImageData> destImageData(luminancedImage->getImageData(intImageRect));

    for (unsigned pixelOffset = 0; pixelOffset < srcPixelArray->length(); pixelOffset++) {
        unsigned pixelByteOffset = pixelOffset * 4;

        unsigned char r = 0, g = 0, b = 0, a = 0;
        srcPixelArray->get(pixelByteOffset, r);
        srcPixelArray->get(pixelByteOffset + 1, g);
        srcPixelArray->get(pixelByteOffset + 2, b);
        srcPixelArray->get(pixelByteOffset + 3, a);

        double luma = (r * 0.2125 + g * 0.7154 + b * 0.0721) * ((double)a / 255.0);

        destImageData->data()->set(pixelByteOffset + 3, luma);
    }

    luminancedImage->putImageData(destImageData.get(), intImageRect, IntPoint(0, 0));

    context->clipToImageBuffer(m_maskRect, luminancedImage.get());
}

TextStream& SVGResourceMasker::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MASKER]";
    return ts;
}

SVGResourceMasker* getMaskerById(Document* document, const AtomicString& id)
{
    SVGResource* resource = getResourceById(document, id);
    if (resource && resource->isMasker())
        return static_cast<SVGResourceMasker*>(resource);

    return 0;
}

} // namespace WebCore

#endif
