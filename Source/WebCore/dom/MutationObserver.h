/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "GCReachableRef.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTMLSlotElement;
class MutationCallback;
class MutationObserverRegistration;
class MutationRecord;
class Node;

using MutationObserverOptions = unsigned char;
using MutationRecordDeliveryOptions = unsigned char;

class MutationObserver final : public RefCounted<MutationObserver> {
    WTF_MAKE_ISO_ALLOCATED(MutationObserver);
    friend class MutationObserverMicrotask;
public:
    enum MutationType {
        ChildList = 1 << 0,
        Attributes = 1 << 1,
        CharacterData = 1 << 2,

        AllMutationTypes = ChildList | Attributes | CharacterData
    };

    enum ObservationFlags  {
        Subtree = 1 << 3,
        AttributeFilter = 1 << 4
    };

    enum DeliveryFlags {
        AttributeOldValue = 1 << 5,
        CharacterDataOldValue = 1 << 6,
    };

    static Ref<MutationObserver> create(Ref<MutationCallback>&&);

    ~MutationObserver();

    struct Init {
        bool childList;
        Optional<bool> attributes;
        Optional<bool> characterData;
        bool subtree;
        Optional<bool> attributeOldValue;
        Optional<bool> characterDataOldValue;
        Optional<Vector<String>> attributeFilter;
    };

    ExceptionOr<void> observe(Node&, const Init&);
    
    struct TakenRecords {
        Vector<Ref<MutationRecord>> records;
        HashSet<GCReachableRef<Node>> pendingTargets;
    };
    TakenRecords takeRecords();
    void disconnect();

    void observationStarted(MutationObserverRegistration&);
    void observationEnded(MutationObserverRegistration&);
    void enqueueMutationRecord(Ref<MutationRecord>&&);
    void setHasTransientRegistration();
    bool canDeliver();

    HashSet<Node*> observedNodes() const;

    MutationCallback& callback() const { return m_callback.get(); }

    static void enqueueSlotChangeEvent(HTMLSlotElement&);

private:
    explicit MutationObserver(Ref<MutationCallback>&&);
    void deliver();

    static void notifyMutationObservers();
    static bool validateOptions(MutationObserverOptions);

    Ref<MutationCallback> m_callback;
    Vector<Ref<MutationRecord>> m_records;
    HashSet<GCReachableRef<Node>> m_pendingTargets;
    HashSet<MutationObserverRegistration*> m_registrations;
    unsigned m_priority;
};

} // namespace WebCore
