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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include "SVGHelper.h"

#include "DocumentImpl.h"
#include "FrameView.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedRectImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGStringListImpl.h"
#include "ksvg.h"
#include <cmath>
#include <kcanvas/KCanvas.h>

using namespace WebCore;
using namespace std;

float SVGHelper::PercentageOfViewport(float value, const SVGElementImpl *viewportElement, LengthMode mode)
{
    float width = 0, height = 0;
    if(!viewportElement)
        return 0.0;
 
    if(viewportElement->isSVG())
    {
        const SVGSVGElementImpl *svg = static_cast<const SVGSVGElementImpl *>(viewportElement);
        if(svg->hasAttribute(SVGNames::viewBoxAttr))
        {
            width = svg->viewBox()->baseVal()->width();
            height = svg->viewBox()->baseVal()->height();
        }
        else if(svg->width()->baseVal()->unitType() == SVG_LENGTHTYPE_PERCENTAGE ||
                svg->height()->baseVal()->unitType() == SVG_LENGTHTYPE_PERCENTAGE)
        {
            // TODO: Shouldn't w/h be multiplied with the percentage values?!
            // AFAIK, this assumes width & height == 100%, Rob??
            DocumentImpl *doc = svg->getDocument();
            if(doc->documentElement() == svg)
            {
                // We have to ask the canvas for the full "canvas size"...
                RenderCanvas *canvas = static_cast<RenderCanvas *>(doc->renderer());
                if(canvas)
                {
                    width = canvas->view()->visibleWidth(); // TODO: recheck!
                    height = canvas->view()->visibleHeight(); // TODO: recheck!
                }
            }
        }
        else
        {
            width = svg->width()->baseVal()->value();
            height = svg->height()->baseVal()->value();
        }
    }

    if(mode == LM_WIDTH)
        return value * width;
    else if(mode == LM_HEIGHT)
        return value * height;
    else if(mode == LM_OTHER)
        return value * sqrt(pow(double(width), 2) + pow(double(height), 2)) / sqrt(2.0);
    
    return 0.0;
}

void SVGHelper::ParseSeperatedList(SVGStringListImpl *list, const QString &data, const QChar &delimiter)
{
    // TODO : more error checking/reporting
    list->clear();

    QStringList substrings = QStringList::split(delimiter, data);
    
    QStringList::ConstIterator it = substrings.begin();
    QStringList::ConstIterator end = substrings.end();
    for(; it != end; ++it)
    {
        DOMStringImpl *string = new DOMStringImpl(*it);
        string->ref();

        list->appendItem(string);
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

