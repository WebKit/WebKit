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

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& value, RefPtr<CSSValue>&& hotSpot)
{
    auto* imageValue = dynamicDowncast<CSSImageValue>(value.get());
    auto originalURL = imageValue ? imageValue->url() : CSS::URL::none();
    return adoptRef(*new CSSCursorImageValue(WTF::move(value), WTF::move(hotSpot), WTF::move(originalURL)));
}

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& imageValue, RefPtr<CSSValue>&& hotSpot, CSS::URL&& originalURL)
{
    return adoptRef(*new CSSCursorImageValue(WTF::move(imageValue), WTF::move(hotSpot), WTF::move(originalURL)));
}

CSSCursorImageValue::CSSCursorImageValue(Ref<CSSValue>&& imageValue, RefPtr<CSSValue>&& hotSpot, CSS::URL&& originalURL)
    : CSSValue(ClassType::CursorImage)
    , m_originalURL(WTF::move(originalURL))
    , m_imageValue(WTF::move(imageValue))
    , m_hotSpot(WTF::move(hotSpot))
{
}

CSSCursorImageValue::~CSSCursorImageValue() = default;

String CSSCursorImageValue::customCSSText(const CSS::SerializationContext& context) const
{
    auto text = m_imageValue->cssText(context);
    if (!m_hotSpot)
        return text;
    return makeString(text, ' ', m_hotSpot->first().cssText(context), ' ', m_hotSpot->second().cssText(context));
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const
{
    return compareCSSValue(m_imageValue, other.m_imageValue)
        && compareCSSValuePtr(m_hotSpot, other.m_hotSpot);
}

RefPtr<Style::CursorImage> CSSCursorImageValue::createStyleImage(const Style::BuilderState& state) const
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
    return Style::CursorImage::create(styleImage.releaseNonNull(), hotSpot, Style::toStyle(m_originalURL, state));
}

} // namespace WebCore
