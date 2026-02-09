/*
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2026 Squarespace, Inc. www.squarespace.com
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
#include "LazyLoadMediaObserver.h"

#include "HTMLMediaElement.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include <limits>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LazyLoadMediaObserver);

class LazyMediaLoadIntersectionObserverCallback final : public IntersectionObserverCallback {
public:
    static Ref<LazyMediaLoadIntersectionObserverCallback> create(Document& document)
    {
        return adoptRef(*new LazyMediaLoadIntersectionObserverCallback(document));
    }

private:
    LazyMediaLoadIntersectionObserverCallback(Document& document)
        : IntersectionObserverCallback(&document)
    {
    }

    bool hasCallback() const final { return true; }

    CallbackResult<void> invoke(IntersectionObserver&, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver&) final
    {
        ASSERT(!entries.isEmpty());

        for (auto& entry : entries) {
            if (!entry->isIntersecting())
                continue;
            if (RefPtr element = dynamicDowncast<HTMLMediaElement>(entry->target()))
                element->loadDeferredMedia();
        }
        return { };
    }

    CallbackResult<void> invokeRethrowingException(IntersectionObserver& thisObserver, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver& observer) final
    {
        return invoke(thisObserver, entries, observer);
    }
};

void LazyLoadMediaObserver::observe(Element& element)
{
    auto& observer = element.document().lazyLoadMediaObserver();
    auto* intersectionObserver = observer.intersectionObserver(protect(element.document()));
    if (!intersectionObserver)
        return;
    intersectionObserver->observe(element);
}

void LazyLoadMediaObserver::unobserve(Element& element, Document& document)
{
    if (auto& observer = document.lazyLoadMediaObserver().m_observer)
        observer->unobserve(element);
}

IntersectionObserver* LazyLoadMediaObserver::intersectionObserver(Document& document)
{
    if (!m_observer) {
        auto callback = LazyMediaLoadIntersectionObserverCallback::create(document);
        static NeverDestroyed<const String> lazyLoadingScrollMarginFallback(MAKE_STATIC_STRING_IMPL("100%"));
        IntersectionObserver::Init options { std::nullopt, { }, lazyLoadingScrollMarginFallback, { } };
        auto observer = IntersectionObserver::create(document, WTF::move(callback), WTF::move(options));
        if (observer.hasException())
            return nullptr;
        m_observer = observer.returnValue().ptr();
    }
    return m_observer.get();
}

bool LazyLoadMediaObserver::isObserved(Element& element) const
{
    return m_observer && m_observer->isObserving(element);
}

}
