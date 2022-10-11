/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCrossfadeImage.h"

#include "CSSCrossfadeValue.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

StyleCrossfadeImage::StyleCrossfadeImage(Ref<CSSCrossfadeValue>&& value)
    : StyleGeneratedImage(Type::CrossfadeImage, value->isFixedSize())
    , m_imageGeneratorValue { WTFMove(value) }
{
}

StyleCrossfadeImage::~StyleCrossfadeImage() = default;

bool StyleCrossfadeImage::operator==(const StyleImage& other) const
{
    if (is<StyleCrossfadeImage>(other))
        return arePointingToEqualData(m_imageGeneratorValue.ptr(), downcast<StyleCrossfadeImage>(other).m_imageGeneratorValue.ptr());
    return false;
}

CSSImageGeneratorValue& StyleCrossfadeImage::imageValue()
{
    return m_imageGeneratorValue;
}

Ref<CSSValue> StyleCrossfadeImage::cssValue() const
{
    return m_imageGeneratorValue.copyRef();
}

bool StyleCrossfadeImage::isPending() const
{
    return m_imageGeneratorValue->isPending();
}

void StyleCrossfadeImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    m_imageGeneratorValue->loadSubimages(loader, options);
}

RefPtr<Image> StyleCrossfadeImage::image(const RenderElement* renderer, const FloatSize& size) const
{
    return renderer ? m_imageGeneratorValue->image(const_cast<RenderElement&>(*renderer), size) : &Image::nullImage();
}

bool StyleCrossfadeImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_imageGeneratorValue->knownToBeOpaque(renderer);
}

FloatSize StyleCrossfadeImage::fixedSize(const RenderElement& renderer) const
{
    return m_imageGeneratorValue->fixedSize(renderer);
}

void StyleCrossfadeImage::addClient(RenderElement& renderer)
{
    m_imageGeneratorValue->addClient(renderer);
}

void StyleCrossfadeImage::removeClient(RenderElement& renderer)
{
    m_imageGeneratorValue->removeClient(renderer);
}

bool StyleCrossfadeImage::hasClient(RenderElement& renderer) const
{
    return m_imageGeneratorValue->clients().contains(&renderer);
}

} // namespace WebCore
