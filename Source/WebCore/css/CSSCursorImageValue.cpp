/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "CSSCursorImageValue.h"

#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "SVGCursorElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLengthContext.h"
#include "SVGURIReference.h"
#include "StyleBuilderState.h"
#include "StyleCursorImage.h"
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    auto originalURL = is<CSSImageValue>(imageValue) ? downcast<CSSImageValue>(imageValue.get()).imageURL() : URL();
    return adoptRef(*new CSSCursorImageValue(WTFMove(imageValue), hotSpot, WTFMove(originalURL), loadedFromOpaqueSource));
}

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, URL originalURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return adoptRef(*new CSSCursorImageValue(WTFMove(imageValue), hotSpot, WTFMove(originalURL), loadedFromOpaqueSource));
}

CSSCursorImageValue::CSSCursorImageValue(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, URL originalURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : CSSValue(CursorImageClass)
    , m_originalURL(WTFMove(originalURL))
    , m_imageValue(WTFMove(imageValue))
    , m_hotSpot(hotSpot)
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

CSSCursorImageValue::~CSSCursorImageValue() = default;

String CSSCursorImageValue::customCSSText() const
{
    auto text = m_imageValue.get().cssText();
    if (!m_hotSpot)
        return text;
    return makeString(text, ' ', m_hotSpot->x(), ' ', m_hotSpot->y());
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const
{
    return m_hotSpot == other.m_hotSpot && compareCSSValue(m_imageValue, other.m_imageValue);
}

RefPtr<StyleImage> CSSCursorImageValue::createStyleImage(Style::BuilderState& state) const
{
    auto styleImage = state.createStyleImage(m_imageValue.get());
    if (!styleImage)
        return nullptr;
    return StyleCursorImage::create(styleImage.releaseNonNull(), m_hotSpot, m_originalURL, m_loadedFromOpaqueSource);
}

} // namespace WebCore
