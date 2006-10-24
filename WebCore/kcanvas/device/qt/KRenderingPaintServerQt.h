/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.LIB. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KRenderingPaintServerQt_H
#define KRenderingPaintServerQt_H

#include "KRenderingPaintServer.h"

class QPen;

namespace WebCore {

class RenderStyle;
class RenderObject;

// This class is designed as an extension to
// KRenderingPaintServer, it won't inherit from it.
class KRenderingPaintServerQt
{
public:
    KRenderingPaintServerQt();
    ~KRenderingPaintServerQt();

    void setPenProperties(const RenderObject*, const RenderStyle*, QPen&) const;
};

}

#endif

// vim:ts=4:noet
