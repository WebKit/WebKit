/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2009 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "Element.h"
#include "Timer.h"
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class DeferredPromise;
class Document;
class ImageLoader;
class Page;
class RenderImageResource;

template<typename T> class EventSender;
typedef EventSender<ImageLoader> ImageEventSender;

class ImageLoader : public CachedImageClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ImageLoader();

    enum class RelevantMutation : bool { Yes, No };

    // This function should be called when the element is attached to a document; starts
    // loading if a load hasn't already been started.
    void updateFromElement(RelevantMutation = RelevantMutation::No);

    // This function should be called whenever the 'src' attribute is set, even if its value
    // doesn't change; starts new load unconditionally (matches Firefox and Opera behavior).
    void updateFromElementIgnoringPreviousError(RelevantMutation = RelevantMutation::No);

    void elementDidMoveToNewDocument(Document&);

    Element& element() { return m_element; }
    const Element& element() const { return m_element; }

    bool imageComplete() const { return m_imageComplete; }

    CachedImage* image() const { return m_image.get(); }
    void clearImage(); // Cancels pending beforeload and load events, and doesn't dispatch new ones.
    
    size_t pendingDecodePromisesCountForTesting() const { return m_decodingPromises.size(); }
    void decode(Ref<DeferredPromise>&&);

    void setLoadManually(bool loadManually) { m_loadManually = loadManually; }

    bool hasPendingBeforeLoadEvent() const { return m_hasPendingBeforeLoadEvent; }
    bool hasPendingActivity() const { return m_hasPendingLoadEvent || m_hasPendingErrorEvent; }

    void dispatchPendingEvent(ImageEventSender*);

    static void dispatchPendingBeforeLoadEvents(Page*);
    static void dispatchPendingLoadEvents(Page*);
    static void dispatchPendingErrorEvents(Page*);

    void loadDeferredImage();

    bool isDeferred() const { return m_lazyImageLoadState == LazyImageLoadState::Deferred; }

    Document& document() { return m_element.document(); }

protected:
    explicit ImageLoader(Element&);
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) override;

private:
    enum class LazyImageLoadState : uint8_t { None, Deferred, LoadImmediately, FullImage };

    virtual void dispatchLoadEvent() = 0;
    virtual String sourceURI(const AtomString&) const = 0;

    void updatedHasPendingEvent();

    void dispatchPendingBeforeLoadEvent();
    void dispatchPendingLoadEvent();
    void dispatchPendingErrorEvent();

    RenderImageResource* renderImageResource();
    void updateRenderer();

    void clearImageWithoutConsideringPendingLoadEvent();
    void clearFailedLoadURL();

    bool hasPendingDecodePromises() const { return !m_decodingPromises.isEmpty(); }
    void resolveDecodePromises();
    void rejectDecodePromises(const char* message);
    void decode();
    
    void timerFired();

    Element& m_element;
    CachedResourceHandle<CachedImage> m_image;
    Timer m_derefElementTimer;
    RefPtr<Element> m_protectedElement;
    AtomString m_failedLoadURL;
    Vector<RefPtr<DeferredPromise>> m_decodingPromises;
    bool m_hasPendingBeforeLoadEvent : 1;
    bool m_hasPendingLoadEvent : 1;
    bool m_hasPendingErrorEvent : 1;
    bool m_imageComplete : 1;
    bool m_loadManually : 1;
    bool m_elementIsProtected : 1;
    LazyImageLoadState m_lazyImageLoadState { LazyImageLoadState::None };
};

}
