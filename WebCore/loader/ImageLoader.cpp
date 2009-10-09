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

class ImageEventSender {
public:
    ImageEventSender(const AtomicString& eventType);

    void dispatchEventSoon(ImageLoader*);
    void cancelEvent(ImageLoader*);

    void dispatchPendingEvents();

private:
    void timerFired(Timer<ImageEventSender>*);

    AtomicString m_eventType;
    Timer<ImageEventSender> m_timer;
    Vector<ImageLoader*> m_dispatchSoonList;
    Vector<ImageLoader*> m_dispatchingList;
};

static ImageEventSender& beforeLoadEventSender()
{
    DEFINE_STATIC_LOCAL(ImageEventSender, sender, (eventNames().beforeloadEvent));
    return sender;
}

static ImageEventSender& loadEventSender()
{
    DEFINE_STATIC_LOCAL(ImageEventSender, sender, (eventNames().loadEvent));
    return sender;
}

ImageLoader::ImageLoader(Element* element)
    : m_element(element)
    , m_image(0)
    , m_firedBeforeLoad(true)
    , m_firedLoad(true)
    , m_imageComplete(true)
    , m_loadManually(false)
{
}

ImageLoader::~ImageLoader()
{
    if (m_image)
        m_image->removeClient(this);
    if (!m_firedBeforeLoad)
        beforeLoadEventSender().cancelEvent(this);
    if (!m_firedLoad)
        loadEventSender().cancelEvent(this);
}

void ImageLoader::setImage(CachedImage* newImage)
{
    ASSERT(m_failedLoadURL.isEmpty());
    CachedImage* oldImage = m_image.get();
    if (newImage != oldImage) {
        setLoadingImage(newImage);
        m_firedBeforeLoad = true;
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
    m_image = loadingImage;
    m_firedBeforeLoad = !loadingImage;
    m_firedLoad = !loadingImage;
    m_imageComplete = !loadingImage;
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
        if (newImage) {
            newImage->addClient(this);
            if (!m_element->document()->hasListenerType(Document::BEFORELOAD_LISTENER))
                dispatchPendingBeforeLoadEvent();
            else
                beforeLoadEventSender().dispatchEventSoon(this);
        }
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
    if (haveFiredBeforeLoadEvent())
        updateRenderer();

    loadEventSender().dispatchEventSoon(this);
}

void ImageLoader::updateRenderer()
{
    if (RenderObject* renderer = m_element->renderer()) {
        if (!renderer->isImage())
            return;
        RenderImage* imageRenderer = toRenderImage(renderer);
        
        // Only update the renderer if it doesn't have an image or if what we have
        // is a complete image.  This prevents flickering in the case where a dynamic
        // change is happening between two images.
        CachedImage* cachedImage = imageRenderer->cachedImage();
        if (m_image != cachedImage && (m_imageComplete || !imageRenderer->cachedImage()))
            imageRenderer->setCachedImage(m_image.get());
    }
}

void ImageLoader::dispatchPendingBeforeLoadEvent()
{
    if (m_firedBeforeLoad)
        return;
    if (!m_image)
        return;
    if (!m_element->document()->attached())
        return;
    m_firedBeforeLoad = true;
    if (m_element->dispatchBeforeLoadEvent(m_image->url())) {
        updateRenderer();
        return;
    }
    if (m_image) {
        m_image->removeClient(this);
        m_image = 0;
    }
    loadEventSender().cancelEvent(this);
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

void ImageLoader::dispatchPendingEvents()
{
    beforeLoadEventSender().dispatchPendingEvents();
    loadEventSender().dispatchPendingEvents();
}

ImageEventSender::ImageEventSender(const AtomicString& eventType)
    : m_eventType(eventType)
    , m_timer(this, &ImageEventSender::timerFired)
{
}

void ImageEventSender::dispatchEventSoon(ImageLoader* loader)
{
    m_dispatchSoonList.append(loader);
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void ImageEventSender::cancelEvent(ImageLoader* loader)
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

void ImageEventSender::dispatchPendingEvents()
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
        if (ImageLoader* loader = m_dispatchingList[i]) {
            if (m_eventType == eventNames().beforeloadEvent)
                loader->dispatchPendingBeforeLoadEvent();
            else
                loader->dispatchPendingLoadEvent();
        }
    }
    m_dispatchingList.clear();
}

void ImageEventSender::timerFired(Timer<ImageEventSender>*)
{
    dispatchPendingEvents();
}

}
