/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
#include "SVGZoomEvent.h"

#include "SVGPoint.h"
#include "SVGRect.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGZoomEvent);

SVGZoomEvent::SVGZoomEvent()
    : m_newScale(0.0f)
    , m_previousScale(0.0f)
{
}

Ref<SVGRect> SVGZoomEvent::zoomRectScreen() const
{
    return SVGRect::create(m_zoomRectScreen);
}

float SVGZoomEvent::previousScale() const
{
    return m_previousScale;
}

void SVGZoomEvent::setPreviousScale(float scale)
{
    m_previousScale = scale;
}

Ref<SVGPoint> SVGZoomEvent::previousTranslate() const
{
    return SVGPoint::create(m_previousTranslate);
}

float SVGZoomEvent::newScale() const
{
    return m_newScale;
}

void SVGZoomEvent::setNewScale(float scale)
{
    m_newScale = scale;
}

Ref<SVGPoint> SVGZoomEvent::newTranslate() const
{
    return SVGPoint::create(m_newTranslate);
}

EventInterface SVGZoomEvent::eventInterface() const
{
    return SVGZoomEventInterfaceType;
}

} // namespace WebCore
