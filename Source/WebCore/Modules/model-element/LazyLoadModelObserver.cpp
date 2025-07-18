/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "LazyLoadModelObserver.h"

#if ENABLE(MODEL_ELEMENT)

#include "DocumentInlines.h"
#include "HTMLModelElement.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "LocalFrame.h"
#include "Logging.h"
#include <limits>
#include <wtf/TZoneMallocInlines.h>
namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LazyLoadModelObserver);

class LazyModelLoadIntersectionObserverCallback final : public IntersectionObserverCallback {
public:
    static Ref<LazyModelLoadIntersectionObserverCallback> create(Document& document)
    {
        return adoptRef(*new LazyModelLoadIntersectionObserverCallback(document));
    }

private:
    LazyModelLoadIntersectionObserverCallback(Document& document)
        : IntersectionObserverCallback(&document)
    {
    }

    bool hasCallback() const final { return true; }

    CallbackResult<void> invoke(IntersectionObserver&, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver&) final
    {
        ASSERT(!entries.isEmpty());

        for (auto& entry : entries) {
            if (RefPtr element = dynamicDowncast<HTMLModelElement>(entry->target()))
                element->viewportIntersectionChanged(entry->isIntersecting());
        }
        return { };
    }

    CallbackResult<void> invokeRethrowingException(IntersectionObserver& thisObserver, const Vector<Ref<IntersectionObserverEntry>>& entries, IntersectionObserver& observer) final
    {
        return invoke(thisObserver, entries, observer);
    }
};

void LazyLoadModelObserver::observe(Element& element)
{
    RefPtr document = element.document();
    if (!document)
        return;

    auto& observer = document->lazyLoadModelObserver();
    RefPtr intersectionObserver = observer.intersectionObserver(*document);
    if (!intersectionObserver)
        return;
    intersectionObserver->observe(element);
}

void LazyLoadModelObserver::unobserve(Element& element, Document& document)
{
    if (auto& intersectionObserver = document.lazyLoadModelObserver().m_observer)
        intersectionObserver->unobserve(element);
}

IntersectionObserver* LazyLoadModelObserver::intersectionObserver(Document& document)
{
    if (!m_observer) {
        auto callback = LazyModelLoadIntersectionObserverCallback::create(document);
        static NeverDestroyed<const String> lazyLoadingScrollMarginFallback(MAKE_STATIC_STRING_IMPL("100%"));
        IntersectionObserver::Init options { std::nullopt, { }, lazyLoadingScrollMarginFallback, { } };
        auto observer = IntersectionObserver::create(document, WTFMove(callback), WTFMove(options));
        if (observer.hasException())
            return nullptr;
        m_observer = observer.returnValue().ptr();
    }
    return m_observer.get();
}

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
