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

#include <kdom/core/AttrImpl.h>
#include <kdom/cache/KDOMLoader.h>
#include "DocLoader.h"
#include "DocumentImpl.h"

#include "SVGNames.h"
#include "SVGImageElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGMatrixImpl.h"
#include "KCanvasRenderingStyle.h"

#include "khtml/rendering/render_image.h"

using namespace KSVG;

SVGImageLoader::SVGImageLoader(SVGImageElementImpl *node) : DOM::HTMLImageLoader(node)
{
}

SVGImageLoader::~SVGImageLoader()
{
}

// FIXME - Refactor most of this code into DOM::HTMLImageLoader or a shared DOM::ImageLoader base class
void SVGImageLoader::updateFromElement()
{
    SVGImageElementImpl *imageElement = static_cast<SVGImageElementImpl *>(element());
    DOM::DocumentImpl* doc = imageElement->ownerDocument();
    
    khtml::CachedImage *newImage = 0;
    if (imageElement->href()->baseVal())
        newImage = doc->docLoader()->requestImage(imageElement->href()->baseVal());

    khtml::CachedImage *oldImage = image();
    if (newImage != oldImage) {
        setLoadingImage(newImage);
        if (newImage)
            newImage->ref(this);
        if (oldImage)
            oldImage->deref(this);
    }

    if (khtml::RenderImage* renderer = static_cast<khtml::RenderImage*>(imageElement->renderer()))
        renderer->resetAnimation();
}
