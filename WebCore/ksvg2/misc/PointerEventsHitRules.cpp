/*
    Copyright (C) 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "PointerEventsHitRules.h"

namespace WebCore {

PointerEventsHitRules::PointerEventsHitRules(ESVGHitTesting hitTesting, EPointerEvents pointerEvents)
{
    if (hitTesting == SVG_PATH_HITTESTING) {
        switch (pointerEvents)
        {
            case PE_VISIBLE_PAINTED:
                requireFill = true;
                requireStroke = true;
            case PE_VISIBLE:
                requireVisible = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PE_VISIBLE_FILL:
                requireVisible = true;
                canHitFill = true;
                break;
            case PE_VISIBLE_STROKE:
                requireVisible = true;
                canHitStroke = true;
                break;
            case PE_PAINTED:
                requireFill = true;
                requireStroke = true;
            case PE_ALL:
                canHitFill = true;
                canHitStroke = true;
                break;
            case PE_FILL:
                canHitFill = true;
                break;
            case PE_STROKE:
                canHitStroke = true;
                break;
            case PE_NONE:
                // nothing to do here, defaults are all false.
                break;
        }
    } else {
        switch (pointerEvents)
        {
            case PE_VISIBLE_PAINTED:
                requireVisible = true;
                requireFill = true;
                requireStroke = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PE_VISIBLE_FILL:
            case PE_VISIBLE_STROKE:
            case PE_VISIBLE:
                requireVisible = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PE_PAINTED:
                requireFill = true;
                requireStroke = true;
                canHitFill = true;
                canHitStroke = true;
                break;
            case PE_FILL:
            case PE_STROKE:
            case PE_ALL:
                canHitFill = true;
                canHitStroke = true;
                break;
            case PE_NONE:
                // nothing to do here, defaults are all false.
                break;
        }
    }
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
