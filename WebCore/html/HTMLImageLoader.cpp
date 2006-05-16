/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "HTMLImageLoader.h"

#include "csshelper.h"
#include "CachedImage.h"
#include "DocLoader.h"
#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "RenderImage.h"

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLImageLoader::HTMLImageLoader(Element* elt)
    : m_element(elt)
    , m_image(0)
    , m_firedLoad(true)
    , m_imageComplete(true)
    , m_loadManually(false)
{
}

HTMLImageLoader::~HTMLImageLoader()
{
    if (m_image)
        m_image->deref(this);
    m_element->document()->removeImage(this);
}

void HTMLImageLoader::setLoadingImage(CachedImage *loadingImage)
{
    m_firedLoad = false;
    m_imageComplete = false;
    m_image = loadingImage;
}

void HTMLImageLoader::updateFromElement()
{
    // If we're not making renderers for the page, then don't load images.  We don't want to slow
    // down the raw HTML parsing case by loading images we don't intend to display.
    Element* elem = element();
    Document* doc = elem->document();
    if (!doc->renderer())
        return;

    AtomicString attr = elem->getAttribute(elem->hasLocalName(objectTag) ? dataAttr : srcAttr);
    
    // Treat a lack of src or empty string for src as no image at all.
    CachedImage *newImage = 0;
    if (!attr.isEmpty()) {
        if (m_loadManually) {
            newImage = new CachedImage(doc->docLoader(), parseURL(attr), KIO::CC_Verify, 0);
            doc->docLoader()->m_docObjects.set(newImage->url(), newImage);
        } else
            newImage = doc->docLoader()->requestImage(parseURL(attr));
    }
    
    CachedImage *oldImage = m_image;
    if (newImage != oldImage) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!doc->ownerElement() && newImage)
            printf("Image requested at %d\n", doc->elapsedTime());
#endif
        setLoadingImage(newImage);
        if (newImage)
            newImage->ref(this);
        if (oldImage)
            oldImage->deref(this);
    }

    if (RenderImage* renderer = static_cast<RenderImage*>(elem->renderer()))
        renderer->resetAnimation();
}

void HTMLImageLoader::dispatchLoadEvent()
{
    if (!m_firedLoad && m_image) {
        m_firedLoad = true;
        element()->dispatchHTMLEvent(m_image->isErrorImage() ? errorEvent : loadEvent, false, false);
    }
}

void HTMLImageLoader::notifyFinished(CachedObject *image)
{
    m_imageComplete = true;
    Element* elem = element();
    Document* doc = elem->document();
    doc->dispatchImageLoadEventSoon(this);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!doc->ownerElement())
            printf("Image loaded at %d\n", doc->elapsedTime());
#endif
    if (RenderImage* renderer = static_cast<RenderImage*>(elem->renderer()))
        renderer->setCachedImage(m_image);
}

}
