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

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseImage.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/CorpseSnapshotDebugInfo.h>
#include <JavaScriptCore/CorpseTargetType.h>
#include <JavaScriptCore/Watchpoint.h>
#include <lldb/API/LLDB.h>
#include <stdexcept>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

#endif // ENABLE(MYA_HEAP)

namespace JSCToolsTest {

#if ENABLE(MYA_HEAP)

namespace {

using JSC::Corpse::Address;
using JSC::Corpse::Snapshot;
using JSC::Corpse::SnapshotDebugInfo;
using JSC::Corpse::TargetCanonicalPointer;
using JSC::Corpse::TargetObject;
using JSC::Corpse::sameClass;
using JSC::Corpse::TargetClass;
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
    uint64_t node;
    // A function whose parameter `staticTypeParameter` points to the class the object is held as.
    uint64_t staticTypeFunction;
    unsigned staticTypeParameter;
};

template<typename Base, typename Class>
Held held(Class& object, Address staticTypeFunction = functionTakingPointerTo<Base>(), unsigned staticTypeParameter = 0)
{
    return { Address { static_cast<Base*>(&object) }.toTargetVMAddress(), Address { &object }.toTargetVMAddress(), staticTypeFunction.toTargetVMAddress(), staticTypeParameter };
}

template<typename Class, typename Base>
Address createHeld()
{
    static NeverDestroyed<Class> object;
    static Held objectHeld = held<Base>(object.get());
    return Address { &objectHeld };
}

// JavaScriptCore's debug info gives the third parameter, after `this` and the VM, as a FireDetail.
Address javaScriptCoreFireDetail()
{
    return memberFunctionAddress(static_cast<void (JSC::WatchpointSet::*)(JSC::VM&, const JSC::FireDetail&)>(&JSC::WatchpointSet::fireAllSlow));
}

constexpr unsigned javaScriptCoreFireDetailParameter = 2;

// FireDetail forbids NeverDestroyed's placement new.
JSC::StringFireDetail& stringFireDetail()
{
    static JSC::StringFireDetail object("debug-info-target");
    return object;
}

Held sameImageObject()
{
    return held<JSC::FireDetail>(stringFireDetail(), javaScriptCoreFireDetail(), javaScriptCoreFireDetailParameter);
}

Held heldThroughExecutable()
{
    return held<JSC::FireDetail>(stringFireDetail());
}

Held crossImageObject()
{
    static CrossImageFireDetail object;
    return held<JSC::FireDetail>(object, javaScriptCoreFireDetail(), javaScriptCoreFireDetailParameter);
}

Address createHeldThroughExecutable()
{
    static Held objectHeld = heldThroughExecutable();
    return Address { &objectHeld };
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

const TargetClass& asClass(const TargetType& type)
{
    return std::get<TargetClass>(type);
}

const TargetType* staticType(SnapshotDebugInfo& debugInfo, const Held& held)
{
    return parameterClass(debugInfo, Address { held.staticTypeFunction }, held.staticTypeParameter);
}

std::optional<TargetCanonicalPointer> canonicalPointer(SnapshotDebugInfo& debugInfo, Snapshot& snapshot, const Held& held)
{
    const TargetType* type = staticType(debugInfo, held);
    if (!type)
        return std::nullopt;
    return debugInfo.canonicalPointer(snapshot, Address { held.object }, *type);
}

// The object is read as the class it is held as, which the caller expects to be reported.
void expectStaticType(Snapshot& snapshot, SnapshotDebugInfo& debugInfo, const Held& held, const char* message)
{
    const TargetType* type = staticType(debugInfo, held);
    if (!type)
        return;
    TargetCanonicalPointer pointer = debugInfo.canonicalPointer(snapshot, Address { held.object }, *type);
    TEST_ASSERT(pointer.node.address == Address { held.object } && &pointer.node.type == type && pointer.subobject.address == pointer.node.address && &pointer.subobject.type == type, message);
}

void expectDynamicType(Snapshot& snapshot, Address heldAddress, ASCIILiteral image, ASCIILiteral name, size_t size)
{
    auto held = snapshot.memory().ptr<Held>(heldAddress);
    TEST_ASSERT(held, "the target's object reads");
    if (!held)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    auto pointer = canonicalPointer(debugInfo.get(), snapshot, *held);
    TEST_ASSERT(pointer, "a polymorphic object has a dynamic type");
    if (!pointer)
        return;
    const TargetObject& object = pointer->node;
    TEST_ASSERT_EQ(asClass(object.type).name, String { name }, "the dynamic type is the class the object is, not the one it is held as");
    TEST_ASSERT(asClass(object.type).image.endsWith(makeString('/', image)), "the dynamic type is the class as the image of the vtable defines it");
    TEST_ASSERT_EQ(asClass(object.type).byteSize, size, "the debug info gives the class its size");
    TEST_ASSERT_HEX_EQ(object.address.toTargetVMAddress(), held->node, "the node starts where its vtable's offset to top says");
    TEST_ASSERT_HEX_EQ(pointer->subobject.address.toTargetVMAddress(), held->object, "the pointer designates the subobject it points to");
    const TargetType* heldAs = staticType(debugInfo.get(), *held);
    TEST_ASSERT(heldAs && sameClass(asClass(pointer->subobject.type), asClass(*heldAs)), "the subobject is the class the object is held as");
    TEST_ASSERT(asClass(pointer->subobject.type).image == asClass(object.type).image, "the subobject is as the node's image defines it");
    auto again = canonicalPointer(debugInfo.get(), snapshot, *held);
    TEST_ASSERT(again && &again->node.type == &object.type && again->node.address == object.address, "a second lookup gives the same TargetClass");
}

constexpr ASCIILiteral executable = "testLibJSCTools"_s;
constexpr ASCIILiteral javaScriptCore = "JavaScriptCore"_s;

void analyzeSameImageObject(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, javaScriptCore, "JSC::StringFireDetail"_s, sizeof(JSC::StringFireDetail));
}

void analyzeHeldThroughExecutable(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, javaScriptCore, "JSC::StringFireDetail"_s, sizeof(JSC::StringFireDetail));
}

void analyzeCrossImageObject(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, executable, "JSCToolsTest::(anonymous namespace)::CrossImageFireDetail"_s, sizeof(CrossImageFireDetail));
}

void analyzeInheritedFirstSlot(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, executable, "JSCToolsTest::(anonymous namespace)::InheritsFirstSlot"_s, sizeof(InheritsFirstSlot));
}

void analyzeNoVirtualDestructor(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, executable, "JSCToolsTest::(anonymous namespace)::OverridesAct"_s, sizeof(OverridesAct));
}

void analyzeSecondBase(Snapshot& snapshot, Address held)
{
    expectDynamicType(snapshot, held, executable, "JSCToolsTest::(anonymous namespace)::TwoBases"_s, sizeof(TwoBases));
}

void expectNoDynamicType(Snapshot& snapshot, const Held& held, const char* message)
{
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    ExpectedErrors expectedErrors;
    expectStaticType(snapshot, debugInfo.get(), held, message);
}

void expectNoDynamicType(Snapshot& snapshot, Address heldAddress, const char* message)
{
    auto held = snapshot.memory().ptr<Held>(heldAddress);
    TEST_ASSERT(held, "the target's object reads");
    if (held)
        expectNoDynamicType(snapshot, *held, message);
}

void analyzeOverridesNothing(Snapshot& snapshot, Address held)
{
    expectNoDynamicType(snapshot, held, "a class whose vtable holds only its base's functions has no dynamic type");
}

Address createHeldAsUnrelatedClass()
{
    static NeverDestroyed<InheritsFirstSlot> object;
    static Held objectHeld = {
        Address { &object.get() }.toTargetVMAddress(), Address { &object.get() }.toTargetVMAddress(),
        functionTakingPointerTo<SecondBase>().toTargetVMAddress(), 0,
    };
    return Address { &objectHeld };
}

void analyzeHeldAsUnrelatedClass(Snapshot& snapshot, Address held)
{
    expectNoDynamicType(snapshot, held, "an object whose vtable is not of the class it is held as has no dynamic type");
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
    expectDynamicType(snapshot, held, "libstdc++.so.6"_s, "std::runtime_error"_s, sizeof(std::runtime_error));
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
    expectStaticType(snapshot, debugInfo.get(), objects->crossImage, "a class of the replaced executable has no type");
    auto pointer = canonicalPointer(debugInfo.get(), snapshot, objects->sameImage);
    TEST_ASSERT(pointer && asClass(pointer->node.type).name == "JSC::StringFireDetail"_s, "a class of JavaScriptCore still has its type");
}

struct NotPolymorphic {
    uint64_t value { 42 };
    Held asFireDetail;
    Held asItself;
};

Address createNotPolymorphic()
{
    static NotPolymorphic object;
    object.asFireDetail = { Address { &object }.toTargetVMAddress(), 0, functionTakingPointerTo<JSC::FireDetail>().toTargetVMAddress(), 0 };
    object.asItself = { Address { &object }.toTargetVMAddress(), 0, functionTakingPointerTo<NotPolymorphic>().toTargetVMAddress(), 0 };
    return Address { &object };
}

void analyzeNoDynamicType(Snapshot& snapshot, Address address)
{
    auto object = snapshot.memory().ptr<NotPolymorphic>(address);
    TEST_ASSERT(object, "the target's object reads");
    if (!object)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    const TargetType* fireDetail = staticType(debugInfo.get(), object->asFireDetail);
    if (!fireDetail)
        return;
    auto isStatic = [&](Address address) {
        ExpectedErrors expectedErrors;
        TargetCanonicalPointer pointer = debugInfo->canonicalPointer(snapshot, address, *fireDetail);
        return pointer.node.address == address && &pointer.node.type == fireDetail;
    };
    TEST_ASSERT(isStatic(Address { }), "a null address has no dynamic type");
    TEST_ASSERT(isStatic(Address { static_cast<uint64_t>(0x10) }), "unreadable memory has no dynamic type");
    TEST_ASSERT(isStatic(address), "an object whose first word is no vtable pointer has no dynamic type");
}

void analyzeNotPolymorphic(Snapshot& snapshot, Address address)
{
    auto object = snapshot.memory().ptr<NotPolymorphic>(address);
    TEST_ASSERT(object, "the target's object reads");
    if (!object)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    const TargetType* type = staticType(debugInfo.get(), object->asItself);
    if (!type)
        return;
    TEST_ASSERT(!asClass(*type).hasVTable, "a class without virtual members has no vtable");
    TargetCanonicalPointer pointer = debugInfo->canonicalPointer(snapshot, address, *type);
    TEST_ASSERT(pointer.node.address == address && &pointer.node.type == type && pointer.subobject.address == address && &pointer.subobject.type == type, "an object of a class without virtual members is its own node");
}

struct HeldThroughEachImage {
    Held throughExecutable;
    Held throughJavaScriptCore;
};

Address createHeldThroughEachImage()
{
    static HeldThroughEachImage objects { heldThroughExecutable(), sameImageObject() };
    return Address { &objects };
}

void analyzeTypesFromEachImage(Snapshot& snapshot, Address address)
{
    auto objects = snapshot.memory().ptr<HeldThroughEachImage>(address);
    TEST_ASSERT(objects, "the target's objects read");
    if (!objects)
        return;
    Ref debugInfo = SnapshotDebugInfo::create(snapshot);
    const TargetType* inExecutable = staticType(debugInfo.get(), objects->throughExecutable);
    const TargetType* inJavaScriptCore = staticType(debugInfo.get(), objects->throughJavaScriptCore);
    TEST_ASSERT(inExecutable && asClass(*inExecutable).image.endsWith(makeString('/', executable)), "this executable's function gives the class as this executable defines it");
    TEST_ASSERT(inJavaScriptCore && asClass(*inJavaScriptCore).image.endsWith(makeString('/', javaScriptCore)), "JavaScriptCore's function gives the class as JavaScriptCore defines it");
    TEST_ASSERT(inExecutable && inJavaScriptCore && sameClass(asClass(*inExecutable), asClass(*inJavaScriptCore)), "two images define one class the same way");
    {
        ExpectedErrors expectedErrors;
        TEST_ASSERT(debugInfo->parameterTypes(Address { static_cast<uint64_t>(0x10) }).isEmpty(), "no function is described at an address no image holds");
    }
}

void analyzeEveryObject(decltype(analyzeInAndOutOfProcess)* analyze)
{
    analyze(createSameImageObject, analyzeSameImageObject);
    analyze(createHeldThroughExecutable, analyzeHeldThroughExecutable);
    analyze(createCrossImageObject, analyzeCrossImageObject);
    analyze(createHeld<InheritsFirstSlot, SlotZeroBase>, analyzeInheritedFirstSlot);
    analyze(createHeld<OverridesAct, NoVirtualDestructor>, analyzeNoVirtualDestructor);
    analyze(createHeld<OverridesNothing, NoVirtualDestructor>, analyzeOverridesNothing);
    analyze(createHeldAsUnrelatedClass, analyzeHeldAsUnrelatedClass);
    analyze(createHeld<TwoBases, SecondBase>, analyzeSecondBase);
    analyze(createSystemLibraryObject, analyzeSystemLibraryObject);
    analyze(createNotPolymorphic, analyzeNoDynamicType);
    analyze(createNotPolymorphic, analyzeNotPolymorphic);
    analyze(createHeldThroughEachImage, analyzeTypesFromEachImage);
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

#else

void testDebugInfo()
{
    SuiteTracer tracer("DebugInfo");
    if (!tracer.shouldRun())
        return;

    skipSuite("DebugInfo", "mya_heap is off, or liblldb's headers were not found");
}

#endif // ENABLE(MYA_HEAP)

} // namespace JSCToolsTest
