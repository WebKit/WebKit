/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ImageLoader.h"

#include "CSSHelper.h"
#include "CachedImage.h"
#include "DocLoader.h"
#include "Document.h"
#include "Element.h"
#include "RenderImage.h"

namespace WebCore {

ImageLoader::ImageLoader(Element* elt)
    : m_element(elt)
    , m_image(0)
    , m_firedLoad(true)
    , m_imageComplete(true)
    , m_loadManually(false)
{
}

ImageLoader::~ImageLoader()
{
    if (m_image)
        m_image->removeClient(this);
    m_element->document()->removeImage(this);
}

void ImageLoader::setImage(CachedImage* newImage)
{
    ASSERT(m_failedLoadURL.isEmpty());
    CachedImage* oldImage = m_image.get();
    if (newImage != oldImage) {
        setLoadingImage(newImage);
        m_firedLoad = true;
        m_imageComplete = true;
        if (newImage)
            newImage->addClient(this);
        if (oldImage)
            oldImage->removeClient(this);
    }

    if (RenderObject* renderer = element()->renderer()) {
        if (!renderer->isImage())
            return;

        static_cast<RenderImage*>(renderer)->resetAnimation();
    }
}

void ImageLoader::setLoadingImage(CachedImage* loadingImage)
{
    m_firedLoad = false;
    m_imageComplete = false;
    m_image = loadingImage;
}

void ImageLoader::updateFromElement()
{
    // If we're not making renderers for the page, then don't load images.  We don't want to slow
    // down the raw HTML parsing case by loading images we don't intend to display.
    Element* elem = element();
    Document* doc = elem->document();
    if (!doc->renderer())
        return;

    AtomicString attr = elem->getAttribute(elem->imageSourceAttributeName());

    if (attr == m_failedLoadURL)
        return;

    // Do not load any image if the 'src' attribute is missing or if it is
    // an empty string referring to a local file. The latter condition is
    // a quirk that preserves old behavior that Dashboard widgets
    // need (<rdar://problem/5994621>).
    CachedImage* newImage = 0;
    if (!(attr.isNull() || attr.isEmpty() && doc->baseURI().isLocalFile())) {
        if (m_loadManually) {
            doc->docLoader()->setAutoLoadImages(false);
            newImage = new CachedImage(sourceURI(attr));
            newImage->setLoading(true);
            newImage->setDocLoader(doc->docLoader());
            doc->docLoader()->m_documentResources.set(newImage->url(), newImage);
        } else
            newImage = doc->docLoader()->requestImage(sourceURI(attr));

        // If we do not have an image here, it means that a cross-site
        // violation occurred.
        m_failedLoadURL = !newImage ? attr : AtomicString();
    }
    
    CachedImage* oldImage = m_image.get();
    if (newImage != oldImage) {
        setLoadingImage(newImage);
        if (newImage)
            newImage->addClient(this);
        if (oldImage)
            oldImage->removeClient(this);
    }

    if (RenderObject* renderer = elem->renderer()) {
        if (!renderer->isImage())
            return;

        static_cast<RenderImage*>(renderer)->resetAnimation();
    }
}

void ImageLoader::updateFromElementIgnoringPreviousError()
{
    // Clear previous error.
    m_failedLoadURL = AtomicString();
    updateFromElement();
}

void ImageLoader::notifyFinished(CachedResource*)
{
    ASSERT(m_failedLoadURL.isEmpty());
    m_imageComplete = true;

    Element* elem = element();
    elem->document()->dispatchImageLoadEventSoon(this);

    if (RenderObject* renderer = elem->renderer()) {
        if (!renderer->isImage())
            return;

        static_cast<RenderImage*>(renderer)->setCachedImage(m_image.get());
    }
}

}
