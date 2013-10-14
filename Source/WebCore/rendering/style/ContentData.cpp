/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
    
    ContentData* lastNewData = result.get();
    for (const ContentData* contentData = next(); contentData; contentData = contentData->next()) {
        auto newData = contentData->cloneInternal();
        lastNewData->setNext(std::move(newData));
        lastNewData = lastNewData->next();
    }
        
    return result;
}

RenderObject* ImageContentData::createRenderer(Document& document, RenderStyle& pseudoStyle) const
{
    RenderImage* image = new (*document.renderArena()) RenderImage(document);
    image->setPseudoStyle(&pseudoStyle);
    if (m_image)
        image->setImageResource(RenderImageResourceStyleImage::create(m_image.get()));
    else
        image->setImageResource(RenderImageResource::create());
    return image;
}

RenderObject* TextContentData::createRenderer(Document& document, RenderStyle&) const
{
    return new (*document.renderArena()) RenderTextFragment(document, m_text);
}

RenderObject* CounterContentData::createRenderer(Document& document, RenderStyle&) const
{
    return new (*document.renderArena()) RenderCounter(document, *m_counter);
}

RenderObject* QuoteContentData::createRenderer(Document& document, RenderStyle&) const
{
    return new (*document.renderArena()) RenderQuote(document, m_quote);
}

} // namespace WebCore
