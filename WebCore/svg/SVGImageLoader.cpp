/*
    Copyright (C) 2005, 2005 Alexander Kellett <lypanov@kde.org>

    This file is part of the WebKit project

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)

#include "Attr.h"
#include "DocLoader.h"
#include "Document.h"

#include "SVGImageElement.h"
#include "SVGLength.h"
#include "SVGNames.h"

#include "RenderImage.h"

namespace WebCore {

SVGImageLoader::SVGImageLoader(SVGImageElement* node)
    : HTMLImageLoader(node)
{
}

SVGImageLoader::~SVGImageLoader()
{
}

// FIXME - Refactor most of this code into WebCore::HTMLImageLoader or a shared WebCore::ImageLoader base class
void SVGImageLoader::updateFromElement()
{
    SVGImageElement *imageElement = static_cast<SVGImageElement *>(element());
    WebCore::Document* doc = imageElement->ownerDocument();
    
    CachedImage *newImage = 0;
    if (!imageElement->href().isEmpty()) {
        DeprecatedString uri = imageElement->baseURI().deprecatedString();
        if (!uri.isEmpty())
            uri = KURL(uri, imageElement->href().deprecatedString()).deprecatedString();
        else
            uri = imageElement->href().deprecatedString();
        newImage = doc->docLoader()->requestImage(uri);
    }

    CachedImage *oldImage = image();
    if (newImage != oldImage) {
        setLoadingImage(newImage);
        if (newImage)
            newImage->ref(this);
        if (oldImage)
            oldImage->deref(this);
    }

    if (RenderImage* renderer = static_cast<RenderImage*>(imageElement->renderer()))
        renderer->resetAnimation();
}

void SVGImageLoader::dispatchLoadEvent()
{
    if (!haveFiredLoadEvent() && image()) {
        setHaveFiredLoadEvent(true);
        if (image()->errorOccurred()) {
            // FIXME: We're supposed to put the document in an "error state" per the spec.
        } else
            static_cast<SVGElement*>(element())->sendSVGLoadEventIfPossible(true);
    }
}

}

#endif // ENABLE(SVG)
