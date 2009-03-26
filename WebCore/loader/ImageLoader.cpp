/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

class ImageLoadEventSender {
public:
    ImageLoadEventSender();

    void dispatchLoadEventSoon(ImageLoader*);
    void cancelLoadEvent(ImageLoader*);

    void dispatchPendingLoadEvents();

private:
    ~ImageLoadEventSender();

    void timerFired(Timer<ImageLoadEventSender>*);

    Timer<ImageLoadEventSender> m_timer;
    Vector<ImageLoader*> m_dispatchSoonList;
    Vector<ImageLoader*> m_dispatchingList;
};

static ImageLoadEventSender& loadEventSender()
{
    DEFINE_STATIC_LOCAL(ImageLoadEventSender, sender, ());
    return sender;
}

ImageLoader::ImageLoader(Element* element)
    : m_element(element)
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
    loadEventSender().cancelLoadEvent(this);
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

    if (RenderObject* renderer = m_element->renderer()) {
        if (!renderer->isImage())
            return;

        toRenderImage(renderer)->resetAnimation();
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
    Document* document = m_element->document();
    if (!document->renderer())
        return;

    AtomicString attr = m_element->getAttribute(m_element->imageSourceAttributeName());

    if (attr == m_failedLoadURL)
        return;

    // Do not load any image if the 'src' attribute is missing or if it is
    // an empty string referring to a local file. The latter condition is
    // a quirk that preserves old behavior that Dashboard widgets
    // need (<rdar://problem/5994621>).
    CachedImage* newImage = 0;
    if (!(attr.isNull() || (attr.isEmpty() && document->baseURI().isLocalFile()))) {
        if (m_loadManually) {
            document->docLoader()->setAutoLoadImages(false);
            newImage = new CachedImage(sourceURI(attr));
            newImage->setLoading(true);
            newImage->setDocLoader(document->docLoader());
            document->docLoader()->m_documentResources.set(newImage->url(), newImage);
        } else
            newImage = document->docLoader()->requestImage(sourceURI(attr));

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

    if (RenderObject* renderer = m_element->renderer()) {
        if (!renderer->isImage())
            return;

        toRenderImage(renderer)->resetAnimation();
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

    loadEventSender().dispatchLoadEventSoon(this);

    if (RenderObject* renderer = m_element->renderer()) {
        if (!renderer->isImage())
            return;

        toRenderImage(renderer)->setCachedImage(m_image.get());
    }
}

void ImageLoader::dispatchPendingLoadEvent()
{
    if (m_firedLoad)
        return;
    if (!m_image)
        return;
    if (!m_element->document()->attached())
        return;
    m_firedLoad = true;
    dispatchLoadEvent();
}

void ImageLoader::dispatchPendingLoadEvents()
{
    loadEventSender().dispatchPendingLoadEvents();
}

ImageLoadEventSender::ImageLoadEventSender()
    : m_timer(this, &ImageLoadEventSender::timerFired)
{
}

void ImageLoadEventSender::dispatchLoadEventSoon(ImageLoader* loader)
{
    m_dispatchSoonList.append(loader);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void ImageLoadEventSender::cancelLoadEvent(ImageLoader* loader)
{
    // Remove instances of this loader from both lists.
    // Use loops because we allow multiple instances to get into the lists.
    size_t size = m_dispatchSoonList.size();
    for (size_t i = 0; i < size; ++i) {
        if (m_dispatchSoonList[i] == loader)
            m_dispatchSoonList[i] = 0;
    }
    size = m_dispatchingList.size();
    for (size_t i = 0; i < size; ++i) {
        if (m_dispatchingList[i] == loader)
            m_dispatchingList[i] = 0;
    }
    if (m_dispatchSoonList.isEmpty())
        m_timer.stop();
}

void ImageLoadEventSender::dispatchPendingLoadEvents()
{
    // Need to avoid re-entering this function; if new dispatches are
    // scheduled before the parent finishes processing the list, they
    // will set a timer and eventually be processed.
    if (!m_dispatchingList.isEmpty())
        return;

    m_timer.stop();

    m_dispatchingList.swap(m_dispatchSoonList);
    size_t size = m_dispatchingList.size();
    for (size_t i = 0; i < size; ++i) {
        if (ImageLoader* loader = m_dispatchingList[i])
            loader->dispatchPendingLoadEvent();
    }
    m_dispatchingList.clear();
}

void ImageLoadEventSender::timerFired(Timer<ImageLoadEventSender>*)
{
    dispatchPendingLoadEvents();
}

}
