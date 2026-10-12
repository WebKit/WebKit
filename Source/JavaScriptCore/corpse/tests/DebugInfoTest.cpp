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
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/CorpseSnapshotDebugInfo.h>
#include <JavaScriptCore/CorpseTargetType.h>
#include <JavaScriptCore/Watchpoint.h>
#include <lldb/API/LLDB.h>
#include <stdexcept>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

#endif // HAVE(LLDB)

namespace JSCToolsTest {

#if HAVE(LLDB)

namespace {

using JSC::Corpse::Address;
using JSC::Corpse::Snapshot;
using JSC::Corpse::SnapshotDebugInfo;
using JSC::Corpse::TargetType;

// Its vtable is in this executable, and its base's in JavaScriptCore.
class CrossImageFireDetail final : public JSC::FireDetail {
public:
    void dump(PrintStream&) const final { }
};

// InheritsFirstSlot's first slot is SlotZeroBase::first.
class SlotZeroBase {
public:
    virtual void first() { }
    virtual ~SlotZeroBase() = default;
};

class InheritsFirstSlot final : public SlotZeroBase {
public:
    ~InheritsFirstSlot() final = default;
};

// No destructor is in these vtables.
class NoVirtualDestructor {
public:
    virtual void act() { }

protected:
    ~NoVirtualDestructor() = default;
};

class OverridesAct final : public NoVirtualDestructor {
public:
    void act() final { }
};

class OverridesNothing final : public NoVirtualDestructor {
};

// Held through SecondBase, whose vtable has a nonzero offset to top.
class SecondBase {
public:
    virtual ~SecondBase() = default;

    uint64_t m_second { 2 };
};

class TwoBases final : public SlotZeroBase, public SecondBase {
public:
    ~TwoBases() final = default;
};

struct Held {
    uint64_t object; // As a pointer to a base.
    uint64_t completeObject;
};

template<typename Base, typename Class>
Held held(Class& object)
{
    return { Address { static_cast<Base*>(&object) }.toTargetVMAddress(), Address { &object }.toTargetVMAddress() };
}

template<typename Class, typename Base>
Address createHeld()
{
    static NeverDestroyed<Class> object;
    static Held objectHeld = held<Base>(object.get());
    return Address { &objectHeld };
}

// FireDetail forbids NeverDestroyed's placement new.
Held sameImageObject()
{
    static JSC::StringFireDetail object("debug-info-target");
    return held<JSC::FireDetail>(object);
}

Held crossImageObject()
{
    static CrossImageFireDetail object;
    return held<JSC::FireDetail>(object);
}

Address createSameImageObject()
{
    static Held objectHeld = sameImageObject();
    return Address { &objectHeld };
}

Address createCrossImageObject()
{
    static Held objectHeld = crossImageObject();
    return Address { &objectHeld };
}

void expectDynamicType(Snapshot& snapshot, Address heldAddress, ASCIILiteral name, size_t size)
{
    auto held = snapshot.memory().ptr<Held>(heldAddress);
    TEST_ASSERT(held, "the target's object reads");
    if (!held)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    Address completeObject;
    const TargetType* type = debugInfo->dynamicTypeAt(snapshot, Address { held->object }, completeObject);
    TEST_ASSERT(type, "a polymorphic object has a dynamic type");
    if (!type)
        return;
    TEST_ASSERT_EQ(type->name(), String { name }, "the dynamic type is the class the object is, not the one it is held as");
    TEST_ASSERT_EQ(type->byteSize(), size, "the debug info gives the class its size");
    TEST_ASSERT_HEX_EQ(completeObject.toTargetVMAddress(), held->completeObject, "the complete object starts where its vtable's offset to top says");
    Address again;
    TEST_ASSERT(debugInfo->dynamicTypeAt(snapshot, Address { held->object }, again) == type && again == completeObject, "a second lookup gives the same TargetType");
}

void analyzeSameImageObject(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, "JSC::StringFireDetail"_s, sizeof(JSC::StringFireDetail));
}

void analyzeCrossImageObject(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, "JSCToolsTest::(anonymous namespace)::CrossImageFireDetail"_s, sizeof(CrossImageFireDetail));
}

void analyzeInheritedFirstSlot(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, "JSCToolsTest::(anonymous namespace)::InheritsFirstSlot"_s, sizeof(InheritsFirstSlot));
}

void analyzeNoVirtualDestructor(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, "JSCToolsTest::(anonymous namespace)::OverridesAct"_s, sizeof(OverridesAct));
}

void analyzeSecondBase(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, "JSCToolsTest::(anonymous namespace)::TwoBases"_s, sizeof(TwoBases));
}

void expectNoDynamicType(Snapshot& snapshot, Address heldAddress, const char* message)
{
    auto held = snapshot.memory().ptr<Held>(heldAddress);
    TEST_ASSERT(held, "the target's object reads");
    if (!held)
        return;
    ExpectedErrors expectedErrors;
    Address completeObject;
    TEST_ASSERT(!SnapshotDebugInfo::create(snapshot)->dynamicTypeAt(snapshot, Address { held->object }, completeObject), message);
}

void analyzeOverridesNothing(Snapshot& snapshot, Address held)
{
    expectNoDynamicType(snapshot, held, "a class whose vtable holds only its base's functions has no dynamic type");
}

Address createSystemLibraryObject()
{
    static NeverDestroyed<std::runtime_error> object("debug-info-system-library");
    static Held objectHeld = held<std::exception>(object.get());
    return Address { &objectHeld };
}

void analyzeSystemLibraryObject(Snapshot& snapshot, Address held)
{
#if OS(DARWIN)
    // The shared cache's images are not listed.
    expectNoDynamicType(snapshot, held, "a class in the shared cache has no dynamic type");
#else
    // From libstdc++6-dbgsym.
    expectDynamicType(snapshot, held, "std::runtime_error"_s, sizeof(std::runtime_error));
#endif
}

struct ObjectsInBothImages {
    Held crossImage;
    Held sameImage;
};

Address createObjectsInBothImages()
{
    static ObjectsInBothImages objects { crossImageObject(), sameImageObject() };
    return Address { &objects };
}

// On Darwin liblldb refuses the replaced file, whose UUID differs; on Linux the image is not listed.
void analyzeObjectsInReplacedExecutable(Snapshot& snapshot, Address address)
{
    auto objects = snapshot.memory().ptr<ObjectsInBothImages>(address);
    TEST_ASSERT(objects, "the target's objects read");
    if (!objects)
        return;
    // Otherwise liblldb finds this executable, which has the copy's UUID, in its module cache.
    lldb::SBDebugger::MemoryPressureDetected();
    ExpectedErrors expectedErrors(2);
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    Address completeObject;
    TEST_ASSERT(!debugInfo->dynamicTypeAt(snapshot, Address { objects->crossImage.object }, completeObject), "a class of the replaced executable has no type");
    const TargetType* type = debugInfo->dynamicTypeAt(snapshot, Address { objects->sameImage.object }, completeObject);
    TEST_ASSERT(type && type->name() == "JSC::StringFireDetail"_s, "a class of JavaScriptCore still has its type");
}

Address createNotPolymorphic()
{
    static uint64_t notPolymorphic = 42;
    return Address { &notPolymorphic };
}

void analyzeNoDynamicType(Snapshot& snapshot, Address notPolymorphic)
{
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    Address completeObject;
    ExpectedErrors expectedErrors(3);
    TEST_ASSERT(!debugInfo->dynamicTypeAt(snapshot, Address { }, completeObject), "a null address has no dynamic type");
    TEST_ASSERT(!debugInfo->dynamicTypeAt(snapshot, Address { static_cast<uint64_t>(0x10) }, completeObject), "unreadable memory has no dynamic type");
    TEST_ASSERT(!debugInfo->dynamicTypeAt(snapshot, notPolymorphic, completeObject), "an object whose first word is no vtable pointer has no dynamic type");
}

void analyzeEveryObject(decltype(analyzeInAndOutOfProcess)* analyze)
{
    analyze(createSameImageObject, analyzeSameImageObject);
    analyze(createCrossImageObject, analyzeCrossImageObject);
    analyze(createHeld<InheritsFirstSlot, SlotZeroBase>, analyzeInheritedFirstSlot);
    analyze(createHeld<OverridesAct, NoVirtualDestructor>, analyzeNoVirtualDestructor);
    analyze(createHeld<OverridesNothing, NoVirtualDestructor>, analyzeOverridesNothing);
    analyze(createHeld<TwoBases, SecondBase>, analyzeSecondBase);
    analyze(createSystemLibraryObject, analyzeSystemLibraryObject);
    analyze(createNotPolymorphic, analyzeNoDynamicType);
}

} // anonymous namespace

void testDebugInfo()
{
    SuiteTracer tracer("DebugInfo");
    if (!tracer.shouldRun())
        return;

    analyzeEveryObject(analyzeInAndOutOfProcess);
    if (!linuxSkip("DebugInfo after its target exits", "a Linux snapshot ends with its target"))
        analyzeEveryObject(analyzeAfterTargetExits);
    analyzeAfterExecutableReplaced(createObjectsInBothImages, analyzeObjectsInReplacedExecutable);
}

#else // No SB API, so there is nothing to ask.

void testDebugInfo()
{
    SuiteTracer tracer("DebugInfo");
    if (!tracer.shouldRun())
        return;

#if ENABLE(MYA_HEAP)
    TEST_ASSERT(false, "mya_heap is enabled but liblldb's headers were not found");
#else
    skipSuite("DebugInfo", "mya_heap is not enabled");
#endif
}

#endif // HAVE(LLDB)

} // namespace JSCToolsTest
