/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KCanvasCreator_H
#define KCanvasCreator_H

#include <kcanvas/KCanvasPath.h>

class KCanvasCreator
{
public:
    KCanvasCreator();
    virtual ~KCanvasCreator();

    static KCanvasCreator *self();

    // Canvas path creation
    KCanvasPath* createRoundedRectangle(float x, float y, float width, float height, float rx, float ry) const;
    KCanvasPath* createRectangle(float x, float y, float width, float height) const;
    KCanvasPath* createEllipse(float cx, float cy, float rx, float ry) const;
    KCanvasPath* createCircle(float cx, float cy, float r) const;
    KCanvasPath* createLine(float x1, float y1, float x2, float y2) const;

private:
    static KCanvasCreator *s_creator;
};

#endif

// vim:ts=4:noet
