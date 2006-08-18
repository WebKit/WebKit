/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Apple Computer, Inc.

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

#ifndef KCanvasPathQt_H
#define KCanvasPathQt_H

#include <QPainterPath>

#include "KCanvasPath.h"

namespace WebCore {

class KCanvasPathQt : public KCanvasPath
{
public:
    KCanvasPathQt();
    virtual ~KCanvasPathQt();

    virtual bool isEmpty() const;

    virtual void moveTo(float x, float y);
    virtual void lineTo(float x, float y);
    virtual void curveTo(float x1, float y1, float x2, float y2, float x3, float y3);
    virtual void closeSubpath();

    virtual FloatRect boundingBox();
    virtual FloatRect strokeBoundingBox(const KRenderingStrokePainter&);
    virtual bool strokeContainsPoint(const FloatPoint&);
    virtual bool containsPoint(const FloatPoint&, KCWindRule);

    // Qt specific stuff
    const QPainterPath &qtPath() const { return m_path; }

private:
    QPainterPath m_path;
};

}

#endif

// vim:ts=4:noet
