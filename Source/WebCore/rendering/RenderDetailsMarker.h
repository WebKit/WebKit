/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef RenderDetailsMarker_h
#define RenderDetailsMarker_h

#include "RenderBox.h"

namespace WebCore {

class RenderDetails;

class RenderDetailsMarker : public RenderBox {
public:
    RenderDetailsMarker(RenderDetails*);

    enum Orientation { Up, Down, Left, Right };

    Orientation orientation() const;

    virtual void computePreferredLogicalWidths();
    virtual void destroy();

private:
    virtual const char* renderName() const { return "RenderDetailsMarker"; }
    virtual bool isDetailsMarker() const { return true; }
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void layout();
    virtual int lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const;

    IntRect getRelativeMarkerRect() const;

    bool isOpen() const;
    Path getCanonicalPath() const;
    Path getPath(const IntPoint& origin) const;

    RenderDetails* m_details;
};

inline RenderDetailsMarker* toRenderDetailsMarker(RenderObject* object)
{
    ASSERT(!object || object->isDetailsMarker());
    return static_cast<RenderDetailsMarker*>(object);
}

inline const RenderDetailsMarker* toRenderDetailsMarker(const RenderObject* object)
{
    ASSERT(!object || object->isDetailsMarker());
    return static_cast<const RenderDetailsMarker*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderDetailsMarker(const RenderDetailsMarker*);

}

#endif // RenderDetailsMarker_h

