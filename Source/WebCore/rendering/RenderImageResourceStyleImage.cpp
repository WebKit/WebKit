/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#include "RenderImageResourceStyleImage.h"

#include "CachedImage.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderImageResourceStyleImage);

RenderImageResourceStyleImage::RenderImageResourceStyleImage(StyleImage& styleImage)
    : m_styleImage(styleImage)
{
}

void RenderImageResourceStyleImage::initialize(RenderElement& renderer)
{
    RenderImageResource::initialize(renderer, m_styleImage->hasCachedImage() ? m_styleImage.get().cachedImage() : nullptr);
    m_styleImage->addClient(this->renderer());
}

void RenderImageResourceStyleImage::shutdown()
{
    RenderImageResource::shutdown();
    if (renderer())
        m_styleImage->removeClient(renderer());
}

RefPtr<Image> RenderImageResourceStyleImage::image(const IntSize& size) const
{
    // Generated content may trigger calls to image() while we're still pending, don't assert but gracefully exit.
    if (m_styleImage->isPending())
        return &Image::nullImage();
    if (auto image = m_styleImage->image(renderer(), size))
        return image;
    return &Image::nullImage();
}

void RenderImageResourceStyleImage::setContainerContext(const IntSize& size, const URL&)
{
    ASSERT(renderer());
    m_styleImage->setContainerContextForRenderer(*renderer(), size, renderer()->style().effectiveZoom());
}

} // namespace WebCore
