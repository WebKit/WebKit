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

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGStringListImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGAnimatedRectImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include <kcanvas/KCanvas.h>

#include <cmath>

using namespace KSVG;
using namespace std;

float SVGHelper::PercentageOfViewport(float value, const SVGElementImpl *viewportElement, LengthMode mode)
{
    float width = 0, height = 0;
    if(!viewportElement)
        return 0.0;
 
    const SVGSVGElementImpl *svg = dynamic_cast<const SVGSVGElementImpl *>(viewportElement);
    if(svg)
    {
        KDOM::DOMString viewBox("viewBox");
        if(svg->hasAttribute(viewBox.handle()))
        {
            width = svg->viewBox()->baseVal()->width();
            height = svg->viewBox()->baseVal()->height();
        }
        else if(svg->width()->baseVal()->unitType() == SVG_LENGTHTYPE_PERCENTAGE ||
                svg->height()->baseVal()->unitType() == SVG_LENGTHTYPE_PERCENTAGE)
        {
            // TODO: Shouldn't w/h be multiplied with the percentage values?!
            // AFAIK, this assumes width & height == 100%, Rob??
            SVGDocumentImpl *doc = static_cast<SVGDocumentImpl *>(svg->ownerDocument());
            if(doc && doc->rootElement() == svg)
            {
                // We have to ask the canvas for the full "canvas size"...
                KCanvas *canvas = doc->canvas();
                if(canvas)
                {
                    width = canvas->canvasSize().width(); // TODO: recheck!
                    height = canvas->canvasSize().height(); // TODO: recheck!
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
        KDOM::DOMStringImpl *string = new KDOM::DOMStringImpl(*it);
        string->ref();

        list->appendItem(string);
    }
}

// vim:ts=4:noet
