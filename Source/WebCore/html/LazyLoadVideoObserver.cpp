/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "LazyLoadVideoObserver.h"

#if ENABLE(VIDEO)

#include "HTMLVideoElement.h"
#include "IntersectionObserver.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include <limits>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LazyLoadVideoObserver);

class LazyVideoLoadIntersectionObserverCallback final : public IntersectionObserverCallback {
public:
    static Ref<LazyVideoLoadIntersectionObserverCallback> create(Document& document)
    {
        return adoptRef(*new LazyVideoLoadIntersectionObserverCallback(document));
    }

private:
    LazyVideoLoadIntersectionObserverCallback(Document& document)
        : IntersectionObserverCallback(&document)
    {
    }

    bool NODELETE hasCallback() const final { return true; }

    CallbackResult<void> invoke(IntersectionObserver&, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver&) final
    {
        ASSERT(!entries.isEmpty());

        for (auto& entry : entries) {
            if (RefPtr element = dynamicDowncast<HTMLVideoElement>(entry->target()))
                element->viewportIntersectionChanged(entry->isIntersecting());
        }
        return { };
    }

    CallbackResult<void> invokeRethrowingException(IntersectionObserver& thisObserver, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver& observer) final
    {
        return invoke(thisObserver, entries, observer);
    }
};

LazyLoadVideoObserver::LazyLoadVideoObserver() = default;
LazyLoadVideoObserver::~LazyLoadVideoObserver() = default;

void LazyLoadVideoObserver::observe(HTMLVideoElement& element)
{
    auto& observer = protect(element.document())->lazyLoadVideoObserver();
    RefPtr intersectionObserver = observer.intersectionObserver(protect(element.document()));
    if (!intersectionObserver)
        return;
    intersectionObserver->observe(element);
}

void LazyLoadVideoObserver::unobserve(HTMLVideoElement& element, Document& document)
{
    if (auto& observer = document.lazyLoadVideoObserver().m_observer)
        observer->unobserve(element);
}

IntersectionObserver* LazyLoadVideoObserver::intersectionObserver(Document& document)
{
    if (!m_observer) {
        auto callback = LazyVideoLoadIntersectionObserverCallback::create(document);
        static NeverDestroyed<const String> lazyLoadingScrollMarginFallback(MAKE_STATIC_STRING_IMPL("100%"));
        IntersectionObserver::Init options { std::nullopt, { }, lazyLoadingScrollMarginFallback, { } };
        auto observer = IntersectionObserver::create(document, WTF::move(callback), WTF::move(options));
        if (observer.hasException())
            return nullptr;
        m_observer = observer.returnValue().ptr();
    }
    return m_observer.get();
}

bool LazyLoadVideoObserver::isObserved(HTMLVideoElement& element) const
{
    return m_observer && m_observer->isObserving(element);
}

}

#endif
