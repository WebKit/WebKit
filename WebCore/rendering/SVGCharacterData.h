/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGCharacterData_h
#define SVGCharacterData_h

#if ENABLE(SVG)
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class AffineTransform;
class RenderObject;

// Holds extra data, when the character is laid out on a path
struct SVGCharOnPath : RefCounted<SVGCharOnPath> {
    static PassRefPtr<SVGCharOnPath> create() { return adoptRef(new SVGCharOnPath); }

    float xScale;
    float yScale;

    float xShift;
    float yShift;

    float orientationAngle;

    bool hidden : 1;

private:
    SVGCharOnPath()
        : xScale(1.0f)
        , yScale(1.0f)
        , xShift(0.0f)
        , yShift(0.0f)
        , orientationAngle(0.0f)
        , hidden(false)
    {
    }
};

struct SVGChar {
    SVGChar()
        : x(0.0f)
        , y(0.0f)
        , angle(0.0f)
        , orientationShiftX(0.0f)
        , orientationShiftY(0.0f)
        , drawnSeperated(false)
        , newTextChunk(false)
    {
    }

    float x;
    float y;
    float angle;

    float orientationShiftX;
    float orientationShiftY;

    RefPtr<SVGCharOnPath> pathData;

    // Determines wheter this char needs to be drawn seperated
    bool drawnSeperated : 1;

    // Determines wheter this char starts a new chunk
    bool newTextChunk : 1;

    // Helper methods
    bool isHidden() const;
    AffineTransform characterTransform() const;
};

struct SVGTextDecorationInfo {
    // ETextDecoration is meant to be used here
    HashMap<int, RenderObject*> fillServerMap;
    HashMap<int, RenderObject*> strokeServerMap;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGCharacterData_h
