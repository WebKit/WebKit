/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GCReachableRef.h"
#include "MutationObserver.h"
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace JSC {
class AbstractSlotVisitor;
}

namespace WebCore {

class QualifiedName;

class MutationObserverRegistration : public CanMakeWeakPtr<MutationObserverRegistration> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MutationObserverRegistration(MutationObserver&, Node&, MutationObserverOptions, const HashSet<AtomString>& attributeFilter);
    ~MutationObserverRegistration();

    void resetObservation(MutationObserverOptions, const HashSet<AtomString>& attributeFilter);
    void observedSubtreeNodeWillDetach(Node&);
    std::unique_ptr<HashSet<GCReachableRef<Node>>> takeTransientRegistrations();
    bool hasTransientRegistrations() const { return m_transientRegistrationNodes && !m_transientRegistrationNodes->isEmpty(); }

    bool shouldReceiveMutationFrom(Node&, MutationObserver::MutationType, const QualifiedName* attributeName) const;
    bool isSubtree() const { return m_options & MutationObserver::Subtree; }

    MutationObserver& observer() { return m_observer.get(); }
    Node& node() { return m_node; }
    MutationRecordDeliveryOptions deliveryOptions() const { return m_options & (MutationObserver::AttributeOldValue | MutationObserver::CharacterDataOldValue); }
    MutationObserverOptions mutationTypes() const { return m_options & MutationObserver::AllMutationTypes; }

    bool isReachableFromOpaqueRoots(JSC::AbstractSlotVisitor&) const;

private:
    Ref<MutationObserver> m_observer;
    Node& m_node;
    RefPtr<Node> m_nodeKeptAlive;
    std::unique_ptr<HashSet<GCReachableRef<Node>>> m_transientRegistrationNodes;
    MutationObserverOptions m_options;
    HashSet<AtomString> m_attributeFilter;
};

} // namespace WebCore
