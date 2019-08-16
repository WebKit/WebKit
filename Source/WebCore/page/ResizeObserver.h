/*
 * Copyright (C) 2019 Igalia S.L.
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

#pragma once

#if ENABLE(RESIZE_OBSERVER)

#include "ActiveDOMObject.h"
#include "GCReachableRef.h"
#include "ResizeObservation.h"
#include "ResizeObserverCallback.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Element;

struct ResizeObserverData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Vector<WeakPtr<ResizeObserver>> observers;
};

class ResizeObserver : public RefCounted<ResizeObserver>, public ActiveDOMObject, public CanMakeWeakPtr<ResizeObserver> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ResizeObserver> create(Document&, Ref<ResizeObserverCallback>&&);
    ~ResizeObserver();

    bool hasObservations() const { return m_observations.size(); }
    bool hasActiveObservations() const { return m_activeObservations.size(); }

    void observe(Element&);
    void unobserve(Element&);
    void disconnect();
    void targetDestroyed(Element&);

    static size_t maxElementDepth() { return SIZE_MAX; }
    size_t gatherObservations(size_t depth);
    void deliverObservations();
    bool hasSkippedObservations() const { return m_hasSkippedObservations; }
    void setHasSkippedObservations(bool skipped) { m_hasSkippedObservations = skipped; }

    // ActiveDOMObject.
    bool hasPendingActivity() const override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;

private:
    ResizeObserver(Document&, Ref<ResizeObserverCallback>&&);

    bool removeTarget(Element&);
    void removeAllTargets();
    bool removeObservation(const Element&);

    WeakPtr<Document> m_document;
    RefPtr<ResizeObserverCallback> m_callback;
    Vector<Ref<ResizeObservation>> m_observations;

    Vector<Ref<ResizeObservation>> m_activeObservations;
    Vector<GCReachableRef<Element>> m_pendingTargets;
    bool m_hasSkippedObservations { false };
};

} // namespace WebCore

#endif // ENABLE(RESIZE_OBSERVER)
