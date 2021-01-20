/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InspectorHeapAgent.h"

#include "HeapProfiler.h"
#include "HeapSnapshot.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorEnvironment.h"
#include "JSBigInt.h"
#include "VM.h"
#include <wtf/Stopwatch.h>

namespace Inspector {

using namespace JSC;

InspectorHeapAgent::InspectorHeapAgent(AgentContext& context)
    : InspectorAgentBase("Heap"_s)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_frontendDispatcher(makeUnique<HeapFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(HeapBackendDispatcher::create(context.backendDispatcher, this))
    , m_environment(context.environment)
{
}

InspectorHeapAgent::~InspectorHeapAgent() = default;

void InspectorHeapAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorHeapAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    disable();
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::enable()
{
    if (m_enabled)
        return makeUnexpected("Heap domain already enabled"_s);

    m_enabled = true;

    m_environment.vm().heap.addObserver(this);

    return { };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::disable()
{
    if (!m_enabled)
        return makeUnexpected("Heap domain already disabled"_s);

    m_enabled = false;
    m_tracking = false;

    m_environment.vm().heap.removeObserver(this);

    clearHeapSnapshots();

    return { };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::gc()
{
    VM& vm = m_environment.vm();
    JSLockHolder lock(vm);
    sanitizeStackForVM(vm);
    vm.heap.collectNow(Sync, CollectionScope::Full);

    return { };
}

Protocol::ErrorStringOr<std::tuple<double, Protocol::Heap::HeapSnapshotData>> InspectorHeapAgent::snapshot()
{
    VM& vm = m_environment.vm();
    JSLockHolder lock(vm);

    HeapSnapshotBuilder snapshotBuilder(vm.ensureHeapProfiler());
    snapshotBuilder.buildSnapshot();

    auto timestamp = m_environment.executionStopwatch().elapsedTime().seconds();
    auto snapshotData = snapshotBuilder.json([&] (const HeapSnapshotNode& node) {
        if (Structure* structure = node.cell->structure(vm)) {
            if (JSGlobalObject* globalObject = structure->globalObject()) {
                if (!m_environment.canAccessInspectedScriptState(globalObject))
                    return false;
            }
        }
        return true;
    });

    return { { timestamp, snapshotData } };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::startTracking()
{
    if (m_tracking)
        return { };

    m_tracking = true;

    auto result = snapshot();
    if (!result)
        return makeUnexpected(WTFMove(result.error()));

    auto [timestamp, snapshotData] = WTFMove(result.value());
    m_frontendDispatcher->trackingStart(timestamp, snapshotData);

    return { };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::stopTracking()
{
    if (!m_tracking)
        return { };

    m_tracking = false;

    auto result = snapshot();
    if (!result)
        return makeUnexpected(WTFMove(result.error()));

    auto [timestamp, snapshotData] = WTFMove(result.value());
    m_frontendDispatcher->trackingComplete(timestamp, snapshotData);

    return { };
}

Optional<HeapSnapshotNode> InspectorHeapAgent::nodeForHeapObjectIdentifier(Protocol::ErrorString& errorString, unsigned heapObjectIdentifier)
{
    HeapProfiler* heapProfiler = m_environment.vm().heapProfiler();
    if (!heapProfiler) {
        errorString = "No heap snapshot"_s;
        return WTF::nullopt;
    }

    HeapSnapshot* snapshot = heapProfiler->mostRecentSnapshot();
    if (!snapshot) {
        errorString = "No heap snapshot"_s;
        return WTF::nullopt;
    }

    const Optional<HeapSnapshotNode> optionalNode = snapshot->nodeForObjectIdentifier(heapObjectIdentifier);
    if (!optionalNode) {
        errorString = "No object for identifier, it may have been collected"_s;
        return WTF::nullopt;
    }

    return optionalNode;
}

Protocol::ErrorStringOr<std::tuple<String, RefPtr<Protocol::Debugger::FunctionDetails>, RefPtr<Protocol::Runtime::ObjectPreview>>> InspectorHeapAgent::getPreview(int heapObjectId)
{
    Protocol::ErrorString errorString;

    // Prevent the cell from getting collected as we look it up.
    VM& vm = m_environment.vm();
    JSLockHolder lock(vm);
    DeferGC deferGC(vm.heap);

    unsigned heapObjectIdentifier = static_cast<unsigned>(heapObjectId);
    const Optional<HeapSnapshotNode> optionalNode = nodeForHeapObjectIdentifier(errorString, heapObjectIdentifier);
    if (!optionalNode)
        return makeUnexpected(errorString);

    // String preview.
    JSCell* cell = optionalNode->cell;
    if (cell->isString())
        return { { asString(cell)->tryGetValue(), nullptr, nullptr } };

    // BigInt preview.
    if (cell->isHeapBigInt())
        return { { JSBigInt::tryGetString(vm, asHeapBigInt(cell), 10), nullptr, nullptr } };

    // FIXME: Provide preview information for Internal Objects? CodeBlock, Executable, etc.

    Structure* structure = cell->structure(vm);
    if (!structure)
        return makeUnexpected("Unable to get object details - Structure"_s);

    JSGlobalObject* globalObject = structure->globalObject();
    if (!globalObject)
        return makeUnexpected("Unable to get object details - GlobalObject"_s);

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(globalObject);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Unable to get object details - InjectedScript"_s);

    // Function preview.
    if (cell->inherits<JSFunction>(vm)) {
        RefPtr<Protocol::Debugger::FunctionDetails> functionDetails;
        injectedScript.functionDetails(errorString, cell, functionDetails);
        if (!functionDetails)
            return makeUnexpected(errorString);
        return { { nullString(), functionDetails, nullptr } };
    }

    // Object preview.
    return { { nullString(), nullptr, injectedScript.previewValue(cell) } };
}

Protocol::ErrorStringOr<Ref<Protocol::Runtime::RemoteObject>> InspectorHeapAgent::getRemoteObject(int heapObjectId, const String& objectGroup)
{
    Protocol::ErrorString errorString;

    // Prevent the cell from getting collected as we look it up.
    VM& vm = m_environment.vm();
    JSLockHolder lock(vm);
    DeferGC deferGC(vm.heap);

    unsigned heapObjectIdentifier = static_cast<unsigned>(heapObjectId);
    const Optional<HeapSnapshotNode> optionalNode = nodeForHeapObjectIdentifier(errorString, heapObjectIdentifier);
    if (!optionalNode)
        return makeUnexpected(errorString);

    JSCell* cell = optionalNode->cell;
    Structure* structure = cell->structure(vm);
    if (!structure)
        return makeUnexpected("Unable to get object details - Structure"_s);

    JSGlobalObject* globalObject = structure->globalObject();
    if (!globalObject)
        return makeUnexpected("Unable to get object details - GlobalObject"_s);

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(globalObject);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Unable to get object details - InjectedScript"_s);

    auto object = injectedScript.wrapObject(cell, objectGroup, true);
    if (!object)
        return makeUnexpected("Internal error: unable to cast Object");

    return object.releaseNonNull();
}

static Protocol::Heap::GarbageCollection::Type protocolTypeForHeapOperation(CollectionScope scope)
{
    switch (scope) {
    case CollectionScope::Full:
        return Protocol::Heap::GarbageCollection::Type::Full;
    case CollectionScope::Eden:
        return Protocol::Heap::GarbageCollection::Type::Partial;
    }
    ASSERT_NOT_REACHED();
    return Protocol::Heap::GarbageCollection::Type::Full;
}

void InspectorHeapAgent::willGarbageCollect()
{
    if (!m_enabled)
        return;

    m_gcStartTime = m_environment.executionStopwatch().elapsedTime();
}

void InspectorHeapAgent::didGarbageCollect(CollectionScope scope)
{
    if (!m_enabled) {
        m_gcStartTime = Seconds::nan();
        return;
    }

    if (std::isnan(m_gcStartTime)) {
        // We were not enabled when the GC began.
        return;
    }

    // FIXME: Include number of bytes freed by collection.

    Seconds endTime = m_environment.executionStopwatch().elapsedTime();
    dispatchGarbageCollectedEvent(protocolTypeForHeapOperation(scope), m_gcStartTime, endTime);

    m_gcStartTime = Seconds::nan();
}

void InspectorHeapAgent::clearHeapSnapshots()
{
    VM& vm = m_environment.vm();
    JSLockHolder lock(vm);

    if (HeapProfiler* heapProfiler = vm.heapProfiler()) {
        heapProfiler->clearSnapshots();
        HeapSnapshotBuilder::resetNextAvailableObjectIdentifier();
    }
}

void InspectorHeapAgent::dispatchGarbageCollectedEvent(Protocol::Heap::GarbageCollection::Type type, Seconds startTime, Seconds endTime)
{
    auto protocolObject = Protocol::Heap::GarbageCollection::create()
        .setType(type)
        .setStartTime(startTime.seconds())
        .setEndTime(endTime.seconds())
        .release();

    m_frontendDispatcher->garbageCollected(WTFMove(protocolObject));
}

} // namespace Inspector
