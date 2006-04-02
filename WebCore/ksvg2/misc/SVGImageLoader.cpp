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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT

#include "Attr.h"
#include "DocLoader.h"
#include "Document.h"

#include "SVGNames.h"
#include "SVGImageElement.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGMatrix.h"
#include "KCanvasRenderingStyle.h"

#include "RenderImage.h"

using namespace WebCore;

SVGImageLoader::SVGImageLoader(SVGImageElement *node) : WebCore::HTMLImageLoader(node)
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
    if (imageElement->href()->baseVal())
        newImage = doc->docLoader()->requestImage(imageElement->href()->baseVal());

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
#endif // SVG_SUPPORT

