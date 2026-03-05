/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "BitmapImage.h"
#include "CSSFilterImageValue.h"
#include "CSSFilterRenderer.h"
#include "CSSValuePool.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "NullGraphicsContext.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "Settings.h"
#include "StyleFilter.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

FilterImage::FilterImage(RefPtr<Image>&& image, Filter&& filter)
    : GeneratedImage { Type::FilterImage, FilterImage::isFixedSize }
    , m_image { WTF::move(image) }
    , m_filter { WTF::move(filter) }
    , m_inputImageIsReady { false }
{
}

FilterImage::~FilterImage()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(*this);
}

bool FilterImage::operator==(const Image& other) const
{
    auto* otherFilterImage = dynamicDowncast<FilterImage>(other);
    return otherFilterImage && equals(*otherFilterImage);
}

bool FilterImage::equals(const FilterImage& other) const
{
    return equalInputImages(other) && m_filter == other.m_filter;
}

bool FilterImage::equalInputImages(const FilterImage& other) const
{
    return arePointingToEqualData(m_image, other.m_image);
}

Ref<CSSValue> FilterImage::computedStyleValue(const RenderStyle& style) const
{
    RefPtr image = m_image;
    return CSSFilterImageValue::create(
        image ? image->computedStyleValue(style) : upcast<CSSValue>(CSSPrimitiveValue::create(CSSValueNone)),
        toCSS(m_filter, style)
    );
}

bool FilterImage::isPending() const
{
    RefPtr image = m_image;
    return image && image->isPending();
}

void FilterImage::load(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    CachedResourceHandle<WebCore::CachedImage> oldCachedImage = m_cachedImage;

    if (RefPtr image = m_image) {
        image->load(cachedResourceLoader, options);
        m_cachedImage = image->cachedImage();
    } else
        m_cachedImage = nullptr;

    if (m_cachedImage != oldCachedImage) {
        if (oldCachedImage)
            oldCachedImage->removeClient(*this);
        if (m_cachedImage)
            m_cachedImage->addClient(*this);
    }

    for (auto& value : m_filter) {
        WTF::switchOn(value,
            [&](FilterReference& filterReference) {
                filterReference.loadExternalDocumentIfNeeded(cachedResourceLoader, options);
            },
            []<CSSValueID C, typename T>(FunctionNotation<C, T>&) { }
        );
    }

    m_inputImageIsReady = true;
}

RefPtr<WebCore::Image> FilterImage::image(const RenderElement* renderElement, const FloatSize& size, const GraphicsContext& destinationContext, bool isForFirstLine) const
{
    CheckedPtr renderer = renderElement;
    if (!renderer)
        return &WebCore::Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    RefPtr styleImage = m_image;
    if (!styleImage)
        return &WebCore::Image::nullImage();

    auto image = styleImage->image(renderer.get(), size, destinationContext, isForFirstLine);
    if (!image || image->isNull())
        return &WebCore::Image::nullImage();

    auto preferredFilterRenderingModes = protect(renderer->page())->preferredFilterRenderingModes(destinationContext);
    auto sourceImageRect = FloatRect { { }, size };

    auto cssFilter = CSSFilterRenderer::create(const_cast<RenderElement&>(*renderer), m_filter, {
            .referenceBox = sourceImageRect,
            .filterRegion = sourceImageRect,
            .scale = { 1, 1 },
        }, preferredFilterRenderingModes, Ref { renderer->settings() }->showDebugBorders(), NullGraphicsContext());
    if (!cssFilter)
        return &WebCore::Image::nullImage();

    cssFilter->setFilterRegion(sourceImageRect);

    auto sourceImage = ImageBuffer::create(size, destinationContext.renderingMode(), RenderingPurpose::DOM, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, renderer->hostWindow());
    if (!sourceImage)
        return &WebCore::Image::nullImage();

    auto filteredImage = sourceImage->filteredNativeImage(*cssFilter, [&](GraphicsContext& context) {
        context.drawImage(*image, sourceImageRect);
    });
    if (!filteredImage)
        return &WebCore::Image::nullImage();
    return BitmapImage::create(WTF::move(filteredImage));
}

bool FilterImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

FloatSize FilterImage::fixedSize(const RenderElement& renderer) const
{
    if (RefPtr image = m_image)
        return image->imageSize(&renderer, 1);
    return { };
}

void FilterImage::imageChanged(WebCore::CachedImage*, const IntRect*)
{
    if (!m_inputImageIsReady)
        return;

    for (auto entry : clients()) {
        CheckedRef client = entry.key;
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

} // namespace Style
} // namespace WebCore
