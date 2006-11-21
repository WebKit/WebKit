/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>

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
#include "SVGPaintServerGradient.h"

#include <QColor>
#include <QGradient>

namespace WebCore {

// Helper function used by linear & radial gradient
void SVGPaintServerGradient::fillColorArray(QGradient& gradient, const Vector<SVGGradientStop>& stops, float opacity) const
{
    for (unsigned i = 0; i < stops.size(); ++i) {
        float offset = stops[i].first;
        Color color = stops[i].second;

        QColor c(color.red(), color.green(), color.blue());
        c.setAlpha(int(color.alpha() * opacity));

        gradient.setColorAt(offset, c);
    }
}

} // namespace WebCore

#endif

// vim:ts=4:noet
