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

#ifndef PointerEventsHitRules_H
#define PointerEventsHitRules_H
#ifdef SVG_SUPPORT

#include "SVGRenderStyle.h"

namespace WebCore {

class PointerEventsHitRules {
public:
    enum ESVGHitTesting {
        SVG_IMAGE_HITTESTING,
        SVG_PATH_HITTESTING,
        SVG_TEXT_HITTESTING
    };

    PointerEventsHitRules(ESVGHitTesting, EPointerEvents);

    bool requireVisible;
    bool requireFill;
    bool requireStroke;
    bool canHitStroke;
    bool canHitFill;  
};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
