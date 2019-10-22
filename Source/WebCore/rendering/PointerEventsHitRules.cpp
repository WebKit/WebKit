/*
    Copyright (C) 2007 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "PointerEventsHitRules.h"

namespace WebCore {

PointerEventsHitRules::PointerEventsHitRules(EHitTesting hitTesting, const HitTestRequest& request, PointerEvents pointerEvents)
    : requireVisible(false)
    , requireFill(false)
    , requireStroke(false)
    , canHitStroke(false)
    , canHitFill(false)
    , canHitBoundingBox(false)
{
    if (request.svgClipContent())
        pointerEvents = PointerEvents::Fill;

    if (hitTesting == SVG_PATH_HITTESTING) {
        switch (pointerEvents)
        {
            case PointerEvents::VisiblePainted:
            case PointerEvents::Auto: // "auto" is like "visiblePainted" when in SVG content
                requireFill = true;
                requireStroke = true;
                FALLTHROUGH;
            case PointerEvents::Visible:
                requireVisible = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PointerEvents::VisibleFill:
                requireVisible = true;
                canHitFill = true;
                break;
            case PointerEvents::VisibleStroke:
                requireVisible = true;
                canHitStroke = true;
                break;
            case PointerEvents::Painted:
                requireFill = true;
                requireStroke = true;
                FALLTHROUGH;
            case PointerEvents::All:
                canHitFill = true;
                canHitStroke = true;
                break;
            case PointerEvents::Fill:
                canHitFill = true;
                break;
            case PointerEvents::Stroke:
                canHitStroke = true;
                break;
            case PointerEvents::BoundingBox:
                canHitFill = true;
                canHitBoundingBox = true;
                break;
            case PointerEvents::None:
                // nothing to do here, defaults are all false.
                break;
        }
    } else {
        switch (pointerEvents)
        {
            case PointerEvents::VisiblePainted:
            case PointerEvents::Auto: // "auto" is like "visiblePainted" when in SVG content
                requireVisible = true;
                requireFill = true;
                requireStroke = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PointerEvents::VisibleFill:
            case PointerEvents::VisibleStroke:
            case PointerEvents::Visible:
                requireVisible = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PointerEvents::Painted:
                requireFill = true;
                requireStroke = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PointerEvents::Fill:
            case PointerEvents::Stroke:
            case PointerEvents::All:
                canHitFill = true;
                canHitStroke = true;
                break;
            case PointerEvents::BoundingBox:
                canHitFill = true;
                canHitBoundingBox = true;
                break;
            case PointerEvents::None:
                // nothing to do here, defaults are all false.
                break;
        }
    }
}

}
