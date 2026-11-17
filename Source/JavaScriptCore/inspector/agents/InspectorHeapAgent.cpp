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

#include "config.h"
#include "InspectorHeapAgent.h"

#include "HeapProfiler.h"
#include "HeapSnapshot.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorEnvironment.h"
#include "JSBigInt.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "VM.h"
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace Inspector {

using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorHeapAgent);

// A VM has one chain of heap snapshots, shared by all of its agents (for example those of several
// inspected pages in one web process). Agents only clear it once no agent whose frontend may hold its
// identifiers is left (GC debugging snapshots still clear it on their own). Agents only add themselves
// under the VM's JSLock, which clearHeapSnapshots() also holds, so an agent can't start using the chain
// between the check for users and the clear.
static Lock snapshotUsersLock;

static HashMap<VMIdentifier, Vector<CheckedPtr<InspectorHeapAgent>>>& snapshotUsersByVM() WTF_REQUIRES_LOCK(snapshotUsersLock)
{
    static NeverDestroyed<HashMap<VMIdentifier, Vector<CheckedPtr<InspectorHeapAgent>>>> snapshotUsers;
    return snapshotUsers;
}

static void addSnapshotUser(const VM& vm, InspectorHeapAgent& agent)
{
    Locker locker { snapshotUsersLock };
    snapshotUsersByVM().ensure(vm.identifier(), [] {
        return Vector<CheckedPtr<InspectorHeapAgent>> { };
    }).iterator->value.append(&agent);
}

static void removeSnapshotUser(InspectorHeapAgent& agent)
{
    Locker locker { snapshotUsersLock };
    snapshotUsersByVM().removeIf([&](auto& entry) {
        entry.value.removeFirst(&agent);
        return entry.value.isEmpty();
    });
}

static bool hasSnapshotUsers(const VM& vm)
{
    Locker locker { snapshotUsersLock };
    return snapshotUsersByVM().contains(vm.identifier());
}

// The frontend asks every target for a snapshot at the same time, and several frame targets can share a VM. So that a
// round costs one full garbage collection rather than one per frame, a build stays available to the VM's other agents
// that allow it, until the RunLoop runs a task posted after the build's first serialization. Every request that reached
// the process before then is dispatched before that task. A shared build is only used if no garbage collection and no
// JavaScript ran since. Native code can only have changed the heap in between while handling messages that arrived
// before the reusing request, which a frontend that sends one round's requests together can't observe.
struct SharedSnapshotBuild {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(SharedSnapshotBuild);

    uint64_t identifier;
    MonotonicTime time;
    std::unique_ptr<HeapSnapshotBuilder> builder;
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(SharedSnapshotBuild);

static Lock sharedSnapshotBuildsLock;
static uint64_t nextSharedSnapshotBuildIdentifier WTF_GUARDED_BY_LOCK(sharedSnapshotBuildsLock) { 1 };

static HashMap<VMIdentifier, std::unique_ptr<SharedSnapshotBuild>>& sharedSnapshotBuildsByVM() WTF_REQUIRES_LOCK(sharedSnapshotBuildsLock)
{
    static NeverDestroyed<HashMap<VMIdentifier, std::unique_ptr<SharedSnapshotBuild>>> sharedSnapshotBuilds;
    return sharedSnapshotBuilds;
}

// Only the VM's own thread adds or removes its entry, so the returned build stays valid until that thread does.
static SharedSnapshotBuild* sharedSnapshotBuild(const VM& vm)
{
    Locker locker { sharedSnapshotBuildsLock };
    return sharedSnapshotBuildsByVM().get(vm.identifier());
}

static uint64_t shareSnapshotBuild(const VM& vm, std::unique_ptr<HeapSnapshotBuilder>&& builder, MonotonicTime time)
{
    uint64_t identifier;
    std::unique_ptr<SharedSnapshotBuild> previousBuild;
    {
        Locker locker { sharedSnapshotBuildsLock };
        identifier = nextSharedSnapshotBuildIdentifier++;
        auto build = makeUnique<SharedSnapshotBuild>(SharedSnapshotBuild { identifier, time, WTF::move(builder) });
        auto addResult = sharedSnapshotBuildsByVM().add(vm.identifier(), nullptr);
        previousBuild = std::exchange(addResult.iterator->value, WTF::move(build));
    }

    // The expiry task only runs if this thread's RunLoop runs.
    ASSERT(RunLoop::isMain());
    RunLoop::currentSingleton().dispatch([vmIdentifier = vm.identifier(), identifier] {
        std::unique_ptr<SharedSnapshotBuild> expiredBuild;
        Locker locker { sharedSnapshotBuildsLock };
        auto& sharedSnapshotBuilds = sharedSnapshotBuildsByVM();
        auto iterator = sharedSnapshotBuilds.find(vmIdentifier);
        if (iterator != sharedSnapshotBuilds.end() && iterator->value->identifier == identifier)
            expiredBuild = sharedSnapshotBuilds.take(iterator);
    });

    return identifier;
}

static void discardSharedSnapshotBuild(const VM& vm)
{
    std::unique_ptr<SharedSnapshotBuild> discardedBuild;
    Locker locker { sharedSnapshotBuildsLock };
    discardedBuild = sharedSnapshotBuildsByVM().take(vm.identifier());
}

static JSGlobalObject* realmForCell(JSCell& cell)
{
    if (const Structure* structure = cell.structure())
        return structure->realm();
    return nullptr;
}

InspectorHeapAgent::InspectorHeapAgent(AgentContext& context)
    : InspectorAgentBase("Heap"_s)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_frontendDispatcher(makeUniqueRef<HeapFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(HeapBackendDispatcher::create(protect(context.backendDispatcher), this))
    , m_environment(context.environment)
{
}

InspectorHeapAgent::~InspectorHeapAgent()
{
    // Other agents leave on disconnect, but a JSContext's agent can still be a user here, because its
    // console.takeHeapSnapshot() works without a frontend. Don't clear the chain: a worker's agents are
    // destroyed after its VM.
    if (m_usesSnapshots)
        removeSnapshotUser(*this);
}

InspectorEnvironment& InspectorHeapAgent::environment() const
{
    return m_environment.get();
}

void InspectorHeapAgent::didCreateFrontendAndBackend()
{
}

void InspectorHeapAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    std::ignore = disable();

    // console.takeHeapSnapshot() sends snapshots even when Heap isn't enabled.
    if (m_usesSnapshots)
        clearHeapSnapshots();
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::enable()
{
    if (m_enabled)
        return makeUnexpected("Heap domain already enabled"_s);

    m_enabled = true;

    if (observesGarbageCollections())
        protect(m_environment)->vm().heap.addObserver(this);

    return { };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::disable()
{
    if (!m_enabled)
        return makeUnexpected("Heap domain already disabled"_s);

    m_enabled = false;
    m_tracking = false;

    // Nothing happens if this agent isn't an observer.
    protect(m_environment)->vm().heap.removeObserver(this);

    clearHeapSnapshots();

    return { };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::gc()
{
    VM& vm = protect(m_environment)->vm();
    JSLockHolder lock(vm);
    sanitizeStackForVM(vm);
    vm.heap.collectNow(Sync, CollectionScope::Full);

    return { };
}

Protocol::ErrorStringOr<std::tuple<double, Protocol::Heap::HeapSnapshotData>> InspectorHeapAgent::snapshot()
{
    VM& vm = protect(m_environment)->vm();
    JSLockHolder lock(vm);

    // Become a user of the VM's snapshot chain before building, so that a clear during the build's garbage
    // collection cannot free the chain that the new snapshot links to. This agent's frontend holds the identifiers
    // of the snapshot below even if another agent built it.
    if (!m_usesSnapshots) {
        addSnapshotUser(vm, *this);
        m_usesSnapshots = true;
    }

    Ref stopwatch = protect(m_environment)->executionStopwatch();

    // JavaScript can change the heap without a new entry while the VM is entered (for example console.takeHeapSnapshot()
    // or a debugger pause), so builds made then are never shared.
    bool sharesBuilds = canShareSnapshotBuilds() && !vm.isEntered();

    HeapSnapshotBuilder* snapshotBuilder = nullptr;
    std::unique_ptr<HeapSnapshotBuilder> newSnapshotBuilder;
    MonotonicTime buildTime;
    double timestamp = 0;
    if (sharesBuilds) {
        SharedSnapshotBuild* sharedBuild = sharedSnapshotBuild(vm);
        if (sharedBuild && sharedBuild->identifier != m_lastSharedSnapshotBuildIdentifier && sharedBuild->builder->isSnapshotOfCurrentHeap()) {
            snapshotBuilder = sharedBuild->builder.get();
            m_lastSharedSnapshotBuildIdentifier = sharedBuild->identifier;
            timestamp = stopwatch->fromMonotonicTime(sharedBuild->time).value_or(stopwatch->elapsedTime()).seconds();
        }
    }

    if (!snapshotBuilder) {
        newSnapshotBuilder = makeUnique<HeapSnapshotBuilder>(vm.ensureHeapProfiler());
        newSnapshotBuilder->buildSnapshot();
        snapshotBuilder = newSnapshotBuilder.get();

        auto [elapsedTime, now] = stopwatch->elapsedTimeAndTimestamp();
        timestamp = elapsedTime.seconds();
        buildTime = now;
    }

    snapshotBuilder->setClient(this);
    auto snapshotData = snapshotBuilder->json(sharesBuilds ? HeapSnapshotBuilder::KeepEdges::Yes : HeapSnapshotBuilder::KeepEdges::No);
    snapshotBuilder->setClient(nullptr);
    m_includedGlobalObjectsForSerialization.clear();

    if (newSnapshotBuilder && sharesBuilds)
        m_lastSharedSnapshotBuildIdentifier = shareSnapshotBuild(vm, WTF::move(newSnapshotBuilder), buildTime);

    return { { timestamp, snapshotData } };
}

Protocol::ErrorStringOr<void> InspectorHeapAgent::startTracking()
{
    if (m_tracking)
        return { };

    m_tracking = true;

    auto result = snapshot();
    if (!result)
        return makeUnexpected(WTF::move(result.error()));

    auto [timestamp, snapshotData] = WTF::move(result.value());
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
        return makeUnexpected(WTF::move(result.error()));

    auto [timestamp, snapshotData] = WTF::move(result.value());
    m_frontendDispatcher->trackingComplete(timestamp, snapshotData);

    return { };
}

std::optional<HeapSnapshotNode> InspectorHeapAgent::nodeForHeapObjectIdentifier(Protocol::ErrorString& errorString, unsigned heapObjectIdentifier)
{
    HeapProfiler* heapProfiler = protect(m_environment)->vm().heapProfiler();
    if (!heapProfiler) {
        errorString = "No heap snapshot"_s;
        return std::nullopt;
    }

    HeapSnapshot* snapshot = heapProfiler->mostRecentSnapshot();
    if (!snapshot) {
        errorString = "No heap snapshot"_s;
        return std::nullopt;
    }

    const std::optional<HeapSnapshotNode> optionalNode = snapshot->nodeForObjectIdentifier(heapObjectIdentifier);
    if (!optionalNode) {
        errorString = "No object for identifier, it may have been collected"_s;
        return std::nullopt;
    }

    // The VM's snapshots also hold the cells that other agents of the VM report.
    if (!includesCell(*optionalNode->cell)) {
        errorString = "No object for identifier in this target"_s;
        return std::nullopt;
    }

    return optionalNode;
}

Protocol::ErrorStringOr<std::tuple<String, RefPtr<Protocol::Debugger::FunctionDetails>, RefPtr<Protocol::Runtime::ObjectPreview>>> InspectorHeapAgent::getPreview(int heapObjectId)
{
    Protocol::ErrorString errorString;

    // Prevent the cell from getting collected as we look it up.
    VM& vm = protect(m_environment)->vm();
    JSLockHolder lock(vm);
    DeferGC deferGC(vm);

    unsigned heapObjectIdentifier = static_cast<unsigned>(heapObjectId);
    const std::optional<HeapSnapshotNode> optionalNode = nodeForHeapObjectIdentifier(errorString, heapObjectIdentifier);
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

    Structure* structure = cell->structure();
    if (!structure)
        return makeUnexpected("Unable to get object details - Structure"_s);

    JSGlobalObject* globalObject = structure->realm();
    if (!globalObject)
        return makeUnexpected("Unable to get object details - GlobalObject"_s);

    if (!canCreateInjectedScript(*globalObject))
        return makeUnexpected("Unable to get object details - Inaccessible GlobalObject"_s);

    auto injectedScript = m_injectedScriptManager->injectedScriptFor(globalObject);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Unable to get object details - InjectedScript"_s);

    // Function preview.
    if (cell->inherits<JSFunction>()) {
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
    VM& vm = protect(m_environment)->vm();
    JSLockHolder lock(vm);
    DeferGC deferGC(vm);

    unsigned heapObjectIdentifier = static_cast<unsigned>(heapObjectId);
    const std::optional<HeapSnapshotNode> optionalNode = nodeForHeapObjectIdentifier(errorString, heapObjectIdentifier);
    if (!optionalNode)
        return makeUnexpected(errorString);

    JSCell* cell = optionalNode->cell;
    Structure* structure = cell->structure();
    if (!structure)
        return makeUnexpected("Unable to get object details - Structure"_s);

    JSGlobalObject* globalObject = structure->realm();
    if (!globalObject)
        return makeUnexpected("Unable to get object details - GlobalObject"_s);

    if (!canCreateInjectedScript(*globalObject))
        return makeUnexpected("Unable to get object details - Inaccessible GlobalObject"_s);

    auto injectedScript = m_injectedScriptManager->injectedScriptFor(globalObject);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Unable to get object details - InjectedScript"_s);

    auto object = injectedScript.wrapObject(cell, objectGroup, true);
    if (!object)
        return makeUnexpected("Internal error: unable to cast Object"_s);

    return object.releaseNonNull();
}

NODELETE static Protocol::Heap::GarbageCollection::Type protocolTypeForHeapOperation(CollectionScope scope)
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

    m_gcStartTime = protect(m_environment)->executionStopwatch().elapsedTime();
}

void InspectorHeapAgent::didGarbageCollect(CollectionScope scope)
{
    if (!m_enabled) {
        m_gcStartTime = Seconds::nan();
        return;
    }

    if (m_gcStartTime.isNaN()) {
        // We were not enabled when the GC began.
        return;
    }

    // FIXME: Include number of bytes freed by collection.

    Seconds endTime = protect(m_environment)->executionStopwatch().elapsedTime();
    dispatchGarbageCollectedEvent(protocolTypeForHeapOperation(scope), m_gcStartTime, endTime);

    m_gcStartTime = Seconds::nan();
}

bool InspectorHeapAgent::heapSnapshotBuilderIgnoreNode(const HeapSnapshotBuilder&, JSC::JSCell* cell)
{
    JSGlobalObject* globalObject = realmForCell(*cell);
    if (!globalObject)
        return !includesCell(*cell);

    return !m_includedGlobalObjectsForSerialization.ensure(globalObject, [&] {
        return includesGlobalObject(*globalObject);
    }).iterator->value;
}

bool InspectorHeapAgent::includesGlobalObject(JSGlobalObject& globalObject) const
{
    return protect(m_environment)->canAccessInspectedScriptState(&globalObject);
}

bool InspectorHeapAgent::includesCell(JSCell& cell) const
{
    if (JSGlobalObject* globalObject = realmForCell(cell))
        return includesGlobalObject(*globalObject);

    // FIXME: <https://webkit.org/b/331565> Every agent of a VM includes the cells that belong to no realm, such as
    // strings, also those that only other agents' realms use.
    return true;
}

void InspectorHeapAgent::clearHeapSnapshots()
{
    VM& vm = protect(m_environment)->vm();
    JSLockHolder lock(vm);

    if (m_usesSnapshots) {
        removeSnapshotUser(*this);
        m_usesSnapshots = false;
    }

    if (hasSnapshotUsers(vm))
        return;

    // The shared build could not be used after the clear anyway, so this only frees it earlier.
    discardSharedSnapshotBuild(vm);

    if (HeapProfiler* heapProfiler = vm.heapProfiler())
        heapProfiler->clearSnapshots();
}

void InspectorHeapAgent::dispatchGarbageCollectedEvent(Protocol::Heap::GarbageCollection::Type type, Seconds startTime, Seconds endTime)
{
    auto protocolObject = Protocol::Heap::GarbageCollection::create()
        .setType(type)
        .setStartTime(startTime.seconds())
        .setEndTime(endTime.seconds())
        .release();

    m_frontendDispatcher->garbageCollected(WTF::move(protocolObject));
}

} // namespace Inspector
