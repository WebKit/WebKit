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

#include "GCReachableRef.h"
#include "ResizeObservation.h"
#include "ResizeObserverCallback.h"
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>

namespace JSC {

class AbstractSlotVisitor;

}

namespace WebCore {

class Document;
class Element;
struct ResizeObserverOptions;

struct ResizeObserverData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ResizeObserverData);
    Vector<WeakPtr<ResizeObserver>> observers;
};

using NativeResizeObserverCallback = void (*)(const Vector<Ref<ResizeObserverEntry>>&, ResizeObserver&);
using JSOrNativeResizeObserverCallback = Variant<Ref<ResizeObserverCallback>, NativeResizeObserverCallback>;

class ResizeObserver : public RefCountedAndCanMakeWeakPtr<ResizeObserver> {
    WTF_MAKE_TZONE_ALLOCATED(ResizeObserver);
public:
    static Ref<ResizeObserver> create(Document&, Ref<ResizeObserverCallback>&&);
    static Ref<ResizeObserver> createNativeObserver(Document&, NativeResizeObserverCallback&&);
    ~ResizeObserver();

    bool hasObservations() const { return m_observations.size(); }
    bool hasActiveObservations() const { return m_activeObservations.size(); }

    void observe(Element&);
    void observe(Element&, const ResizeObserverOptions&);
    void unobserve(Element&);
    void disconnect();
    void targetDestroyed(Element&);

    static size_t maxElementDepth() { return SIZE_MAX; }
    size_t gatherObservations(size_t depth);
    void deliverObservations();
    bool hasSkippedObservations() const { return m_hasSkippedObservations; }
    void setHasSkippedObservations(bool skipped) { m_hasSkippedObservations = skipped; }

    void resetObservationSize(Element&);

    const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>& activeObservationTargets() const LIFETIME_BOUND WTF_REQUIRES_LOCK(m_observationTargetsLock) { return m_activeObservationTargets; }
    const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>& targetsWaitingForFirstObservation() const LIFETIME_BOUND WTF_REQUIRES_LOCK(m_observationTargetsLock) { return m_targetsWaitingForFirstObservation; }
    Lock& observationTargetsLock() LIFETIME_BOUND WTF_RETURNS_LOCK(m_observationTargetsLock) { return m_observationTargetsLock; }

    ResizeObserverCallback* callbackConcurrently();
    bool isReachableFromOpaqueRoots(JSC::AbstractSlotVisitor&) const;

private:
    ResizeObserver(Document&, JSOrNativeResizeObserverCallback&&);

    bool removeTarget(Element&);
    void removeAllTargets();
    bool removeObservation(const Element&);
    void observeInternal(Element&, const ResizeObserverBoxOptions);
    bool NODELETE isNativeCallback();
    bool NODELETE isJSCallback();

    struct ResizeObservationHashFunctions {
        using T = Ref<ResizeObservation>;
        using PtrType = const ResizeObservation*;

        static unsigned hash(const PtrType observation) { return PtrHash<Element*>::hash(observation->target()); }
        static bool equal(const PtrType a, const PtrType b) { return a->target() == b->target(); }
        static const bool safeToCompareToEmptyOrDeleted = true;

        static unsigned hash(const T& observation) { return hash(observation.ptr()); }
        static bool equal(const T& a, const T& b) { return equal(a.ptr(), b.ptr()); }
        static bool equal(const PtrType a, const T& b) { return equal(a, b.ptr()); }
        static bool equal(const T& a, const PtrType b) { return equal(a.ptr(), b); }
    };

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    const JSOrNativeResizeObserverCallback m_JSOrNativeCallback;
    ListHashSet<Ref<ResizeObservation>, ResizeObservationHashFunctions> m_observations;
    WeakHashMap<Element, Ref<ResizeObservation>, WeakPtrImplWithEventTargetData> m_observationMap;

    Vector<Ref<ResizeObservation>> m_activeObservations;
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> m_activeObservationTargets WTF_GUARDED_BY_LOCK(m_observationTargetsLock);
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> m_targetsWaitingForFirstObservation WTF_GUARDED_BY_LOCK(m_observationTargetsLock);

    mutable Lock m_observationTargetsLock;
    bool m_hasSkippedObservations { false };
};

} // namespace WebCore
