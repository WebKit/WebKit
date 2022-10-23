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
#include "StyleImageSet.h"

#include "CSSImageSetValue.h"
#include "CSSPrimitiveValue.h"
#include "Document.h"
#include "Page.h"

namespace WebCore {

Ref<StyleImageSet> StyleImageSet::create(Vector<ImageWithScale> images)
{ 
    return adoptRef(*new StyleImageSet(WTFMove(images)));
}

StyleImageSet::StyleImageSet(Vector<ImageWithScale>&& images)
    : StyleMultiImage { Type::ImageSet }
    , m_images { WTFMove(images) }
{
}

StyleImageSet::~StyleImageSet() = default;

bool StyleImageSet::operator==(const StyleImage& other) const
{
    return is<StyleImageSet>(other) && equals(downcast<StyleImageSet>(other));
}

bool StyleImageSet::equals(const StyleImageSet& other) const
{
    return m_images == other.m_images && StyleMultiImage::equals(other);
}

Ref<CSSValue> StyleImageSet::computedStyleValue(const RenderStyle& style) const
{
    auto result = CSSImageSetValue::create();

    for (auto& image : m_images) {
        result->append(image.image->computedStyleValue(style));
        result->append(CSSPrimitiveValue::create(image.scaleFactor, CSSUnitType::CSS_DPPX));
    }

    return result;
}

ImageWithScale StyleImageSet::selectBestFitImage(const Document& document)
{
    updateDeviceScaleFactor(document);

    if (!m_accessedBestFitImage) {
        m_accessedBestFitImage = true;
        m_bestFitImage = bestImageForScaleFactor();
    }

    return m_bestFitImage;
}

ImageWithScale StyleImageSet::bestImageForScaleFactor()
{
    ImageWithScale result;
    for (auto& image : m_images) {
        if (image.scaleFactor >= m_deviceScaleFactor)
            return image;
        result = image;
    }
    return result;
}

void StyleImageSet::updateDeviceScaleFactor(const Document& document)
{
    // FIXME: In the future, we want to take much more than deviceScaleFactor into acount here.
    // All forms of scale should be included: Page::pageScaleFactor(), Frame::pageZoomFactor(),
    // and any CSS transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
    float deviceScaleFactor = document.page() ? document.page()->deviceScaleFactor() : 1;
    if (deviceScaleFactor == m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;
    m_accessedBestFitImage = false;
}

} // namespace WebCore
