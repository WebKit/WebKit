/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& value, RefPtr<CSSValue>&& hotSpot, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    auto* imageValue = dynamicDowncast<CSSImageValue>(value.get());
    auto originalURL = imageValue ? imageValue->imageURL() : URL();
    return adoptRef(*new CSSCursorImageValue(WTFMove(value), WTFMove(hotSpot), WTFMove(originalURL), loadedFromOpaqueSource));
}

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& imageValue, RefPtr<CSSValue>&& hotSpot, URL originalURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return adoptRef(*new CSSCursorImageValue(WTFMove(imageValue), WTFMove(hotSpot), WTFMove(originalURL), loadedFromOpaqueSource));
}

CSSCursorImageValue::CSSCursorImageValue(Ref<CSSValue>&& imageValue, RefPtr<CSSValue>&& hotSpot, URL originalURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : CSSValue(ClassType::CursorImage)
    , m_originalURL(WTFMove(originalURL))
    , m_imageValue(WTFMove(imageValue))
    , m_hotSpot(WTFMove(hotSpot))
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

CSSCursorImageValue::~CSSCursorImageValue() = default;

String CSSCursorImageValue::customCSSText() const
{
    auto text = m_imageValue->cssText();
    if (!m_hotSpot)
        return text;
    return makeString(text, ' ', m_hotSpot->first().cssText(), ' ', m_hotSpot->second().cssText());
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const
{
    return compareCSSValue(m_imageValue, other.m_imageValue)
        && compareCSSValuePtr(m_hotSpot, other.m_hotSpot);
}

RefPtr<StyleCursorImage> CSSCursorImageValue::createStyleImage(const Style::BuilderState& state) const
{
    auto styleImage = state.createStyleImage(m_imageValue.get());
    if (!styleImage)
        return nullptr;

    std::optional<IntPoint> hotSpot;
    if (m_hotSpot) {
        // FIXME: Should we clamp or round instead of just casting from double to int?
        hotSpot = IntPoint {
            static_cast<int>(downcast<CSSPrimitiveValue>(m_hotSpot->first()).resolveAsNumber(state.cssToLengthConversionData())),
            static_cast<int>(downcast<CSSPrimitiveValue>(m_hotSpot->second()).resolveAsNumber(state.cssToLengthConversionData()))
        };
    }
    return StyleCursorImage::create(styleImage.releaseNonNull(), hotSpot, m_originalURL, m_loadedFromOpaqueSource);
}

} // namespace WebCore
