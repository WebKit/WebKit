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

#ifndef KRenderingPaintServerPatternQt_H
#define KRenderingPaintServerPatternQt_H

#include "KRenderingPaintServerQt.h"
#include "KRenderingPaintServerPattern.h"

namespace WebCore {

class KRenderingPaintServerPatternQt : public KRenderingPaintServerPattern,
                                       public KRenderingPaintServerQt
{
public:
    KRenderingPaintServerPatternQt();
    virtual ~KRenderingPaintServerPatternQt();

    virtual void draw(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
    virtual bool setup(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;
    virtual void teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const;

protected:
    virtual void renderPath(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const;
};

}

#endif

// vim:ts=4:noet
