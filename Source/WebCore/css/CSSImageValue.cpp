/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
#include "CSSImageValue.h"

#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSURLValue.h"
#include "CSSValueKeywords.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "Document.h"
#include "Element.h"
#include "StyleBuilderState.h"
#include "StyleCachedImage.h"

namespace WebCore {

CSSImageValue::CSSImageValue()
    : CSSValue(ClassType::Image)
    , m_isInvalid(true)
{
}

CSSImageValue::CSSImageValue(CSS::URL&& location)
    : CSSValue(ClassType::Image)
    , m_location(WTFMove(location))
{
}

Ref<CSSImageValue> CSSImageValue::create()
{
    return adoptRef(*new CSSImageValue);
}

Ref<CSSImageValue> CSSImageValue::create(CSS::URL location)
{
    return adoptRef(*new CSSImageValue(WTFMove(location)));
}

Ref<CSSImageValue> CSSImageValue::create(WTF::URL imageURL, AtomString initiatorType)
{
    return create(CSS::URL { .specified = imageURL.string(), .resolved = WTFMove(imageURL), .modifiers = { .initiatorType = WTFMove(initiatorType) } });
}

CSSImageValue::~CSSImageValue() = default;

bool CSSImageValue::isLoadedFromOpaqueSource() const
{
    return m_location.modifiers.loadedFromOpaqueSource == LoadedFromOpaqueSource::Yes;
}

RefPtr<StyleImage> CSSImageValue::createStyleImage(const Style::BuilderState& state) const
{
    return state.document().resourceStore()->ensureImage(Style::toStyle(m_location, state));
}

bool CSSImageValue::customMayDependOnBaseURL() const
{
    return WebCore::CSS::mayDependOnBaseURL(m_location);
}

bool CSSImageValue::equals(const CSSImageValue& other) const
{
    return m_location == other.m_location;
}

String CSSImageValue::customCSSText(const CSS::SerializationContext& context) const
{
    if (m_isInvalid)
        return ""_s;

    return CSS::serializationForCSS(context, m_location);
}

Ref<DeprecatedCSSOMValue> CSSImageValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    // We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    return DeprecatedCSSOMPrimitiveValue::create(CSSURLValue::create(m_location), styleDeclaration);
}

} // namespace WebCore
