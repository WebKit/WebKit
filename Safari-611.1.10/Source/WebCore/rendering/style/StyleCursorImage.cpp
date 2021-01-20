/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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
#include "CSSImageSetValue.h"
#include "CachedImage.h"
#include "FloatSize.h"
#include "RenderElement.h"

namespace WebCore {

Ref<StyleCursorImage> StyleCursorImage::create(CSSCursorImageValue& cssValue)
{ 
    return adoptRef(*new StyleCursorImage(cssValue)); 
}

bool StyleCursorImage::operator==(const StyleImage& other) const
{
    return is<StyleCursorImage>(other) && equals(downcast<StyleCursorImage>(other));
}

Ref<CSSValue> StyleCursorImage::cssValue() const
{ 
    return m_cssValue.copyRef(); 
}

StyleCursorImage::StyleCursorImage(CSSCursorImageValue& cssValue)
    : m_cssValue(cssValue)
{
    m_isCursorImage = true;
}

StyleCursorImage::~StyleCursorImage() = default;

ImageWithScale StyleCursorImage::selectBestFitImage(const Document& document) const
{
    return m_cssValue->selectBestFitImage(document);
}

void StyleCursorImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom)
{
    if (!hasCachedImage())
        return;
    cachedImage()->setContainerContextForClient(renderer, LayoutSize(containerSize), containerZoom, m_cssValue->imageURL());
}

}
