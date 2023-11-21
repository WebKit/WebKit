/*
 * Copyright (C) 2020 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LazyLoadImageObserver.h"

#include "HTMLImageElement.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "LocalFrame.h"
#include <limits>

namespace WebCore {

class LazyImageLoadIntersectionObserverCallback final : public IntersectionObserverCallback {
public:
    static Ref<LazyImageLoadIntersectionObserverCallback> create(Document& document)
    {
        return adoptRef(*new LazyImageLoadIntersectionObserverCallback(document));
    }

private:
    bool hasCallback() const final { return true; }

    CallbackResult<void> handleEvent(IntersectionObserver&, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver&) final
    {
        ASSERT(!entries.isEmpty());

        for (auto& entry : entries) {
            if (!entry->isIntersecting())
                continue;
            if (RefPtr element = dynamicDowncast<HTMLImageElement>(entry->target())) {
                element->loadDeferredImage();
                element->document().lazyLoadImageObserver().unobserve(*element, element->document());
            }
        }
        return { };
    }

    LazyImageLoadIntersectionObserverCallback(Document& document)
        : IntersectionObserverCallback(&document)
    {
    }
};

void LazyLoadImageObserver::observe(Element& element)
{
    auto& observer = element.document().lazyLoadImageObserver();
    auto* intersectionObserver = observer.intersectionObserver(element.document());
    if (!intersectionObserver)
        return;
    intersectionObserver->observe(element);
}

void LazyLoadImageObserver::unobserve(Element& element, Document& document)
{
    if (auto& observer = document.lazyLoadImageObserver().m_observer)
        observer->unobserve(element);
}

IntersectionObserver* LazyLoadImageObserver::intersectionObserver(Document& document)
{
    if (!m_observer) {
        auto callback = LazyImageLoadIntersectionObserverCallback::create(document);
        static NeverDestroyed<const String> lazyLoadingRootMarginFallback(MAKE_STATIC_STRING_IMPL("100%"));
        IntersectionObserver::Init options { &document, lazyLoadingRootMarginFallback, { } };
        auto observer = IntersectionObserver::create(document, WTFMove(callback), WTFMove(options));
        if (observer.hasException())
            return nullptr;
        m_observer = observer.returnValue().ptr();
    }
    return m_observer.get();
}

bool LazyLoadImageObserver::isObserved(Element& element) const
{
    return m_observer && m_observer->isObserving(element);
}

}
