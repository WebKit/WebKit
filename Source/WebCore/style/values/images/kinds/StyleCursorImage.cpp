/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StyleCursorImage.h"

#include "CSSCursorImageValue.h"
#include "CSSImageValue.h"
#include "CSSValuePair.h"
#include "CachedImage.h"
#include "DeprecatedCSSOMValue.h"
#include "FloatSize.h"
#include "RenderElement.h"
#include "StyleBuilderState.h"
#include "StyleCachedImage.h"
#include "StyleImageSet.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CursorImage);

Ref<CursorImage> CursorImage::create(const Ref<Image>& image, std::optional<HotSpot> hotSpot, const URL& originalURL)
{
    return adoptRef(*new CursorImage(image, hotSpot, originalURL));
}

Ref<CursorImage> CursorImage::create(Ref<Image>&& image, std::optional<HotSpot> hotSpot, URL&& originalURL)
{
    return adoptRef(*new CursorImage(WTF::move(image), hotSpot, WTF::move(originalURL)));
}

CursorImage::CursorImage(const Ref<Image>& image, std::optional<HotSpot> hotSpot, const URL& originalURL)
    : MultiImage { Type::CursorImage }
    , m_image { image }
    , m_hotSpot { hotSpot }
    , m_originalURL { originalURL }
{
}

CursorImage::CursorImage(Ref<Image>&& image, std::optional<HotSpot> hotSpot, URL&& originalURL)
    : MultiImage { Type::CursorImage }
    , m_image { WTF::move(image) }
    , m_hotSpot { hotSpot }
    , m_originalURL { WTF::move(originalURL) }
{
}

CursorImage::~CursorImage() = default;

bool CursorImage::operator==(const Image& other) const
{
    auto* otherCursorImage = dynamicDowncast<CursorImage>(other);
    return otherCursorImage && equals(*otherCursorImage);
}

bool CursorImage::equals(const CursorImage& other) const
{
    return equalInputImages(other) && MultiImage::equals(other);
}

bool CursorImage::equalInputImages(const CursorImage& other) const
{
    return arePointingToEqualData(m_image, other.m_image);
}

Ref<CSSValue> CursorImage::computedStyleValue(const RenderStyle& style) const
{
    return CSSCursorImageValue::create(
        m_image->computedStyleValue(style),
        toCSS(m_hotSpot, style),
        toCSS(m_originalURL, style)
    );
}

Ref<DeprecatedCSSOMValue> CursorImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const RenderStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

ImageWithScale CursorImage::selectBestFitImage(const Document& document)
{
    using namespace CSS::Literals;

    if (RefPtr imageSet = dynamicDowncast<ImageSet>(m_image.get()))
        return imageSet->selectBestFitImage(document);

    return { m_image.ptr(), 1_css_dppx, std::nullopt };
}

void CursorImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom, const WTF::URL& url)
{
    if (!hasCachedImage())
        return;
    cachedImage()->setContainerContextForClient(renderer.cachedImageClient(), LayoutSize(containerSize), containerZoom, !url.isNull() ? url : m_originalURL.resolved);
}

bool CursorImage::usesDataProtocol() const
{
    return m_originalURL.resolved.protocolIsData();
}

} // namespace Style
} // namespace WebCore
