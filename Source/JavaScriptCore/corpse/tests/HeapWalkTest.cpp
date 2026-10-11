/*
 * Copyright (C) 2026 Igalia S.L.
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

#include "LibJSCToolsTestUtilities.h"
#include <JavaScriptCore/CorpsePlatform.h>

#if HAVE(LLDB)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseHeapWalk.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/CorpseSnapshotDebugInfo.h>
#include <JavaScriptCore/CorpseTargetType.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RunLoop.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#endif // HAVE(LLDB)

namespace JSCToolsTest {

#if HAVE(LLDB)

namespace {

using JSC::Corpse::Address;
using JSC::Corpse::HeapAnalyzer;
using JSC::Corpse::Snapshot;
using JSC::Corpse::SnapshotDebugInfo;
using JSC::Corpse::TargetObject;
using JSC::Corpse::TargetType;

// Held only in types with readers of their own, so the walk does not reach them.
struct Unreached {
    uint64_t value { 0 };
};

struct UnreachedNode : BasicRawSentinelNode<UnreachedNode> {
};

struct UnreachedInVirtualBase {
    Unreached* pointer { new Unreached };
};

struct WithVirtualBase : virtual UnreachedInVirtualBase {
};

// Polymorphic, so that the walk finds its type from its vtable.
class MyaRoots {
public:
    explicit MyaRoots(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
        : vm(&vm)
        , globalObject(globalObject)
    {
        vector.append(Unreached { });
        deque.append(Unreached { });
        hashMap.add(1, Unreached { });
        list.push(new UnreachedNode);
    }
    virtual ~MyaRoots() = default;

    JSC::VM* vm;
    JSC::JSGlobalObject* globalObject;
    // Only declared here, though polymorphic.
    JSC::GCActivityCallback* activityCallback { vm->heap.fullActivityCallback() };
    MyaRoots* self { this };
    Vector<Unreached> vector;
    Deque<Unreached> deque;
    HashMap<int, Unreached> hashMap;
    SentinelLinkedList<UnreachedNode> list;
    union {
        Unreached* pointer { new Unreached };
        uint64_t bits;
    } inUnion;
    WithVirtualBase withVirtualBase;
};

struct HeapWalkFixture {
    uint64_t roots;
    uint64_t vm;
    uint64_t apiLock;
    uint64_t runLoop;
    uint64_t activityCallback;
    uint64_t globalObject;
    uint64_t isoSubspace;
    uint64_t gigacageAllocator;
};

Address createFixture()
{
    static HeapWalkFixture fixture;

    JSC::initialize();
    JSC::VM& vm = JSC::VM::create(JSC::HeapType::Large).leakRef();
    setTargetVM(vm);
    JSC::JSLockHolder locker(vm);
    JSC::JSGlobalObject* globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));
    MyaRoots* roots = new MyaRoots(vm, globalObject);
    RELEASE_ASSERT(roots->activityCallback);

    fixture = {
        Address { roots }.toTargetVMAddress(),
        Address { &vm }.toTargetVMAddress(),
        Address { &vm.apiLock() }.toTargetVMAddress(),
        Address { &vm.runLoop() }.toTargetVMAddress(),
        Address { roots->activityCallback }.toTargetVMAddress(),
        Address { globalObject }.toTargetVMAddress(),
        Address { &vm.heap.structureSpace }.toTargetVMAddress(),
        Address { vm.heap.primitiveGigacageAllocator.get() }.toTargetVMAddress(),
    };
    return Address { &fixture };
}

class RecordingAnalyzer final : public HeapAnalyzer {
public:
    void analyzeNode(const TargetObject& object) final
    {
        auto& types = nodes.ensure(object.address.toTargetVMAddress(), [] { return Vector<const TargetType*> { }; }).iterator->value;
        for (const TargetType* type : types)
            TEST_ASSERT(type->image() != object.type->image() || type->name() != object.type->name(), "each image describes each object once");
        types.append(object.type);
    }

    void analyzeEdge(const TargetObject& from, const TargetObject& to) final
    {
        ++edgeCount;
        TEST_ASSERT(isNode(from) && isNode(to), "an edge joins objects the walk reached");
        if (from.address.toTargetVMAddress() == rootAddress)
            fromRoot.append(to);
    }

    static const TargetType* find(const Vector<const TargetType*>& types, const String& name)
    {
        for (const TargetType* type : types) {
            if (type->name() == name)
                return type;
        }
        return nullptr;
    }

    bool isNode(const TargetObject& object) const
    {
        auto it = nodes.find(object.address.toTargetVMAddress());
        return it != nodes.end() && it->value.contains(object.type);
    }

    const TargetType* reached(uint64_t address, ASCIILiteral name) const
    {
        auto it = nodes.find(address);
        return it == nodes.end() ? nullptr : find(it->value, name);
    }

    uint64_t rootAddress { 0 };
    Vector<TargetObject> fromRoot;
    HashMap<uint64_t, Vector<const TargetType*>> nodes;
    size_t edgeCount { 0 };
};

void analyze(Snapshot& snapshot, Address fixtureAddress)
{
    auto fixture = snapshot.memory().ptr<HeapWalkFixture>(fixtureAddress);
    TEST_ASSERT(fixture, "the target's fixture reads");
    if (!fixture)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    Address roots;
    const TargetType* rootsType = debugInfo->dynamicTypeAt(snapshot, Address { fixture->roots }, roots);
    TEST_ASSERT(rootsType, "the roots have a dynamic type");
    if (!rootsType)
        return;

    RecordingAnalyzer analyzer;
    analyzer.rootAddress = fixture->roots;
    JSC::Corpse::walkHeap(snapshot, debugInfo.get(), { roots, rootsType }, analyzer);
    if (verbose)
        dataLogLn("    the walk reached ", analyzer.nodes.size(), " addresses through ", analyzer.edgeCount, " edges");

    TEST_ASSERT(analyzer.reached(fixture->roots, "JSCToolsTest::(anonymous namespace)::MyaRoots"_s), "the walk reaches its root");
    TEST_ASSERT(analyzer.reached(fixture->vm, "JSC::VM"_s), "the walk reaches the VM");
    auto vmTypes = analyzer.nodes.get(fixture->vm);
    TEST_ASSERT(vmTypes.size() == 2 && vmTypes[0]->image() != vmTypes[1]->image(), "the walk reads the VM as this executable and as JavaScriptCore describe it");
    TEST_ASSERT(analyzer.reached(fixture->globalObject, "JSC::JSGlobalObject"_s), "the walk reaches the global object");
    TEST_ASSERT(analyzer.reached(fixture->apiLock, "JSC::JSLock"_s), "the walk reaches the VM's lock through its Ref");
    TEST_ASSERT(analyzer.reached(fixture->runLoop, "WTF::RunLoop"_s), "the walk reaches the VM's run loop through its Ref");
    TEST_ASSERT(analyzer.fromRoot.containsIf([&](auto& to) {
        return to.address.toTargetVMAddress() == fixture->activityCallback && to.type->name() == "JSC::FullGCActivityCallback"_s;
    }), "the walk reads the vtable of an object held through a class only declared where it is held");
    for (auto& types : analyzer.nodes.values()) {
        for (const TargetType* type : types) {
            if (type->name().contains("Unreached"_s))
                TEST_ASSERT_EQ(type->name(), String { }, "the walk does not look inside a type with a reader of its own");
        }
    }
}

// liblldb 21 calls JSC::IsoSubspace incomplete once it has read the compile unit of
// JSC::GigacageAlignedMemoryAllocator's destructor. liblldb keeps the types it read for the
// life of this process, so this runs before any other analysis reads JSC::IsoSubspace.
void analyzeInLLDBOrder(Snapshot& snapshot, Address fixtureAddress)
{
    auto fixture = snapshot.memory().ptr<HeapWalkFixture>(fixtureAddress);
    TEST_ASSERT(fixture, "the target's fixture reads");
    if (!fixture)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    Address completeObject;
    const TargetType* allocator = debugInfo->dynamicTypeAt(snapshot, Address { fixture->gigacageAllocator }, completeObject);
    TEST_ASSERT(allocator && allocator->name() == "JSC::GigacageAlignedMemoryAllocator"_s, "the allocator has its dynamic type");
    const TargetType* isoSubspace = debugInfo->dynamicTypeAt(snapshot, Address { fixture->isoSubspace }, completeObject);
    TEST_ASSERT(isoSubspace && isoSubspace->name() == "JSC::IsoSubspace"_s, "an IsoSubspace has its dynamic type after the allocator");
}

} // anonymous namespace

void testHeapWalk()
{
    SuiteTracer tracer("HeapWalk");
    if (!tracer.shouldRun())
        return;

    analyzeInAndOutOfProcess(createFixture, analyzeInLLDBOrder);
    analyzeInAndOutOfProcess(createFixture, analyze);
}

#else // No SB API, so there is nothing to walk.

void testHeapWalk()
{
    SuiteTracer tracer("HeapWalk");
    if (!tracer.shouldRun())
        return;

#if ENABLE(MYA_HEAP)
    TEST_ASSERT(false, "mya_heap is enabled but liblldb's headers were not found");
#else
    skipSuite("HeapWalk", "mya_heap is not enabled");
#endif
}

#endif // HAVE(LLDB)

} // namespace JSCToolsTest
