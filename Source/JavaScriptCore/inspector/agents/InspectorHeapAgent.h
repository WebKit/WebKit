/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/HeapObserver.h>
#include <JavaScriptCore/HeapSnapshotBuilder.h>
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace JSC {
class JSCell;
class JSGlobalObject;
struct HeapSnapshotNode;
}

namespace Inspector {

class InjectedScriptManager;
class InspectorEnvironment;

class JS_EXPORT_PRIVATE InspectorHeapAgent : public InspectorAgentBase, public HeapBackendDispatcherHandler, public JSC::HeapObserver, public JSC::HeapSnapshotBuilder::Client {
    WTF_MAKE_NONCOPYABLE(InspectorHeapAgent);
    WTF_MAKE_TZONE_ALLOCATED(InspectorHeapAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorHeapAgent);
public:
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);

    InspectorHeapAgent(AgentContext&);
    ~InspectorHeapAgent() override;

    // InspectorAgentBase
    void didCreateFrontendAndBackend() override;
    void willDestroyFrontendAndBackend(DisconnectReason) override;

    // HeapBackendDispatcherHandler
    Protocol::ErrorStringOr<void> enable() override;
    Protocol::ErrorStringOr<void> disable() override;
    Protocol::ErrorStringOr<void> gc() final;
    Protocol::ErrorStringOr<std::tuple<double, Protocol::Heap::HeapSnapshotData>> snapshot() final;
    Protocol::ErrorStringOr<void> startTracking() final;
    Protocol::ErrorStringOr<void> stopTracking() final;
    Protocol::ErrorStringOr<std::tuple<String, RefPtr<Protocol::Debugger::FunctionDetails>, RefPtr<Protocol::Runtime::ObjectPreview>>> getPreview(int heapObjectId) final;
    Protocol::ErrorStringOr<Ref<Protocol::Runtime::RemoteObject>> getRemoteObject(int heapObjectId, const String& objectGroup) final;

    // JSC::HeapObserver
    void willGarbageCollect() final;
    void didGarbageCollect(JSC::CollectionScope) final;

    // JSC::HeapSnapshotBuilder::Client
    bool heapSnapshotBuilderIgnoreNode(const JSC::HeapSnapshotBuilder&, JSC::JSCell*) final;

protected:
    // Stops this agent from using its VM's snapshot chain, and clears the chain if no other agent uses it.
    void clearHeapSnapshots();

    virtual void dispatchGarbageCollectedEvent(Protocol::Heap::GarbageCollection::Type, Seconds startTime, Seconds endTime);

    InspectorEnvironment& environment() const;

    // Whether this agent's snapshots and heap object lookups include the cells of the given realm. Cells that
    // belong to no realm are always included.
    virtual bool includesGlobalObject(JSC::JSGlobalObject&) const;

    virtual bool observesGarbageCollections() const { return true; }

private:
    bool includesCell(JSC::JSCell&) const;
    std::optional<JSC::HeapSnapshotNode> nodeForHeapObjectIdentifier(Protocol::ErrorString&, unsigned heapObjectIdentifier);

    const CheckedRef<InjectedScriptManager> m_injectedScriptManager;
    const UniqueRef<HeapFrontendDispatcher> m_frontendDispatcher;
    const Ref<HeapBackendDispatcher> m_backendDispatcher;
    WeakRef<InspectorEnvironment> m_environment;

    bool m_enabled { false };
    bool m_tracking { false };
    bool m_usesSnapshots { false };
    Seconds m_gcStartTime { Seconds::nan() };
};

} // namespace Inspector
