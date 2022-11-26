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
#include "StyleFilterImage.h"

#include "CSSFilter.h"
#include "CSSFilterImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "ComputedStyleExtractor.h"
#include "ImageBuffer.h"
#include "NullGraphicsContext.h"
#include "RenderElement.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

// MARK: - StyleFilterImage

StyleFilterImage::StyleFilterImage(RefPtr<StyleImage>&& image, FilterOperations&& filterOperations)
    : StyleGeneratedImage { Type::FilterImage, StyleFilterImage::isFixedSize }
    , m_image { WTFMove(image) }
    , m_filterOperations { WTFMove(filterOperations) }
    , m_inputImageIsReady { false }
{
}

StyleFilterImage::~StyleFilterImage()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(*this);
}

bool StyleFilterImage::operator==(const StyleImage& other) const
{
    return is<StyleFilterImage>(other) && equals(downcast<StyleFilterImage>(other));
}

bool StyleFilterImage::equals(const StyleFilterImage& other) const
{
    return equalInputImages(other) && m_filterOperations == other.m_filterOperations;
}

bool StyleFilterImage::equalInputImages(const StyleFilterImage& other) const
{
    return arePointingToEqualData(m_image, other.m_image);
}

Ref<CSSValue> StyleFilterImage::computedStyleValue(const RenderStyle& style) const
{
    return CSSFilterImageValue::create(m_image ? m_image->computedStyleValue(style) : static_reference_cast<CSSValue>(CSSPrimitiveValue::createIdentifier(CSSValueNone)), ComputedStyleExtractor::valueForFilter(style, m_filterOperations));
}

bool StyleFilterImage::isPending() const
{
    return m_image ? m_image->isPending() : false;
}

void StyleFilterImage::load(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    CachedResourceHandle<CachedImage> oldCachedImage = m_cachedImage;

    if (m_image) {
        m_image->load(cachedResourceLoader, options);
        m_cachedImage = m_image->cachedImage();
    } else
        m_cachedImage = nullptr;

    if (m_cachedImage != oldCachedImage) {
        if (oldCachedImage)
            oldCachedImage->removeClient(*this);
        if (m_cachedImage)
            m_cachedImage->addClient(*this);
    }

    for (auto& filterOperation : m_filterOperations.operations()) {
        if (!is<ReferenceFilterOperation>(filterOperation))
            continue;
        auto& referenceFilterOperation = downcast<ReferenceFilterOperation>(*filterOperation);
        referenceFilterOperation.loadExternalDocumentIfNeeded(cachedResourceLoader, options);
    }

    m_inputImageIsReady = true;
}

RefPtr<Image> StyleFilterImage::image(const RenderElement* renderer, const FloatSize& size) const
{
    if (!renderer)
        return &Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    if (!m_image)
        return &Image::nullImage();

    auto image = m_image->image(renderer, size);
    if (!image || image->isNull())
        return &Image::nullImage();

    auto preferredFilterRenderingModes = renderer->page().preferredFilterRenderingModes();
    auto sourceImageRect = FloatRect { { }, size };

    auto cssFilter = CSSFilter::create(const_cast<RenderElement&>(*renderer), m_filterOperations, preferredFilterRenderingModes, FloatSize { 1, 1 }, Filter::ClipOperation::Intersect, sourceImageRect, NullGraphicsContext());
    if (!cssFilter)
        return &Image::nullImage();

    cssFilter->setFilterRegion(sourceImageRect);

    auto sourceImage = ImageBuffer::create(size, RenderingPurpose::DOM, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, bufferOptionsForRendingMode(cssFilter->renderingMode()), { renderer->hostWindow() });
    if (!sourceImage)
        return &Image::nullImage();

    sourceImage->context().drawImage(*image, sourceImageRect);

    if (auto image = sourceImage->filteredImage(*cssFilter))
        return image;

    return &Image::nullImage();
}

bool StyleFilterImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

FloatSize StyleFilterImage::fixedSize(const RenderElement& renderer) const
{
    if (!m_image)
        return { };

    return m_image->imageSize(&renderer, 1);
}

void StyleFilterImage::imageChanged(CachedImage*, const IntRect*)
{
    if (!m_inputImageIsReady)
        return;

    for (auto& client : clients().values())
        client->imageChanged(static_cast<WrappedImagePtr>(this));
}

} // namespace WebCore
