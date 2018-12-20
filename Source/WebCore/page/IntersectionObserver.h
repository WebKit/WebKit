/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INTERSECTION_OBSERVER)

#include "ActiveDOMObject.h"
#include "GCReachableRef.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include "LengthBox.h"
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class Element;

struct IntersectionObserverRegistration {
    WeakPtr<IntersectionObserver> observer;
    Optional<size_t> previousThresholdIndex;
};

struct IntersectionObserverData {
    // IntersectionObservers for which the element that owns this IntersectionObserverData is the root.
    // An IntersectionObserver is only owned by a JavaScript wrapper. ActiveDOMObject::hasPendingActivity
    // is overridden to keep this wrapper alive while the observer has ongoing observations.
    Vector<WeakPtr<IntersectionObserver>> observers;

    // IntersectionObserverRegistrations for which the element that owns this IntersectionObserverData is the target.
    Vector<IntersectionObserverRegistration> registrations;
};

class IntersectionObserver : public RefCounted<IntersectionObserver>, public ActiveDOMObject, public CanMakeWeakPtr<IntersectionObserver> {
public:
    struct Init {
        Element* root { nullptr };
        String rootMargin;
        Variant<double, Vector<double>> threshold;
    };

    static ExceptionOr<Ref<IntersectionObserver>> create(Document&, Ref<IntersectionObserverCallback>&&, Init&&);

    ~IntersectionObserver();

    Document* trackingDocument() const { return m_root ? &m_root->document() : m_implicitRootDocument.get(); }

    Element* root() const { return m_root; }
    String rootMargin() const;
    const LengthBox& rootMarginBox() const { return m_rootMargin; }
    const Vector<double>& thresholds() const { return m_thresholds; }
    const Vector<Element*> observationTargets() const { return m_observationTargets; }

    void observe(Element&);
    void unobserve(Element&);
    void disconnect();

    struct TakenRecords {
        Vector<Ref<IntersectionObserverEntry>> records;
        Vector<GCReachableRef<Element>> pendingTargets;
    };
    TakenRecords takeRecords();

    void targetDestroyed(Element&);
    bool hasObservationTargets() const { return m_observationTargets.size(); }
    void rootDestroyed();

    bool createTimestamp(DOMHighResTimeStamp&) const;

    void appendQueuedEntry(Ref<IntersectionObserverEntry>&&);
    void notify();

    // ActiveDOMObject.
    bool hasPendingActivity() const override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;

private:
    IntersectionObserver(Document&, Ref<IntersectionObserverCallback>&&, Element* root, LengthBox&& parsedRootMargin, Vector<double>&& thresholds);

    bool removeTargetRegistration(Element&);
    void removeAllTargets();

    WeakPtr<Document> m_implicitRootDocument;
    Element* m_root;
    LengthBox m_rootMargin;
    Vector<double> m_thresholds;
    RefPtr<IntersectionObserverCallback> m_callback;
    Vector<Element*> m_observationTargets;
    Vector<GCReachableRef<Element>> m_pendingTargets;
    Vector<Ref<IntersectionObserverEntry>> m_queuedEntries;
};


} // namespace WebCore

#endif // ENABLE(INTERSECTION_OBSERVER)
