/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "ContentData.h"

#include "RenderCounter.h"
#include "RenderImage.h"
#include "RenderImageResource.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderQuote.h"
#include "RenderStyle.h"
#include "RenderTextFragment.h"
#include "StyleInheritedData.h"

namespace WebCore {

std::unique_ptr<ContentData> ContentData::clone() const
{
    auto result = cloneInternal();
    auto* lastNewData = result.get();
    for (auto* contentData = next(); contentData; contentData = contentData->next()) {
        lastNewData->setNext(contentData->cloneInternal());
        lastNewData = lastNewData->next();
    }
    return result;
}

RenderPtr<RenderObject> ImageContentData::createContentRenderer(Document& document, const RenderStyle& pseudoStyle) const
{
    auto image = createRenderer<RenderImage>(RenderObject::Type::Image, document, RenderStyle::createStyleInheritingFromPseudoStyle(pseudoStyle), const_cast<StyleImage*>(m_image.ptr()));
    image->initializeStyle();
    image->setAltText(altText());
    return image;
}

RenderPtr<RenderObject> TextContentData::createContentRenderer(Document& document, const RenderStyle&) const
{
    auto fragment = createRenderer<RenderTextFragment>(document, m_text);
    fragment->setAltText(altText());
    return fragment;
}

RenderPtr<RenderObject> CounterContentData::createContentRenderer(Document& document, const RenderStyle&) const
{
    return createRenderer<RenderCounter>(document, *m_counter);
}

RenderPtr<RenderObject> QuoteContentData::createContentRenderer(Document& document, const RenderStyle& pseudoStyle) const
{
    auto quote = createRenderer<RenderQuote>(document, RenderStyle::createStyleInheritingFromPseudoStyle(pseudoStyle), m_quote);
    quote->initializeStyle();
    return quote;
}

} // namespace WebCore
