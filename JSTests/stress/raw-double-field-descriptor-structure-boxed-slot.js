//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/07-repro-descriptor-structure-adopts-raw-transition.js and the fix design in that directory's fixes/.
//
// REPRO for bug 07 — a SCRIPT-CREATED raw-double transition is adopted by an ENGINE-INTERNAL structure,
// and the engine's own store into it hits the fail-closed RELEASE_ASSERT. Four lines of ordinary JS,
// default options, no JIT, no typed arrays. RELEASE_ASSERT is not compiled out, so this aborts a
// production release build too.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/07-repro-descriptor-structure-adopts-raw-transition.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> abort
//   --useRawDoubleFieldStorage=0                       -> "PASS"
//   --useDoubleFieldRepresentation=0                   -> "PASS"
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// OBSERVED (staged tree, default options):
//
//   ASSERTION FAILED: store of a non-numeric value into a Double-represented field at offset 0; the caller
//                     must widen the representation with a structure transition first
//                     (see JSObject::putDirectInternal)
//   .../runtime/JSObject.cpp(410) : void JSC::JSObject::putDirectOffsetRawDoubleAware(VM &, Structure &, PropertyOffset, JSValue)
//   2  JSC::JSObject::putDirectOffsetRawDoubleAware(JSC::VM&, JSC::Structure&, int, JSC::JSValue)
//   3  JSC::constructObjectFromPropertyDescriptor(JSC::JSGlobalObject*, JSC::PropertyDescriptor ...)
//
// MECHANISM
//   Three facts compose.
//
//   1. The transition table key MASKS the representation bit out (StructureTransitionTable.h:216-218):
//
//        static constexpr unsigned representationMask = 1; // PropertyAttribute::RepresentationDouble
//        Key(PointerKey impl, unsigned attributesIncludingRepresentation, TransitionKind transitionKind)
//            : m_encodedData(impl.raw()
//                | (static_cast<uintptr_t>(attributesIncludingRepresentation & ~representationMask) << attributesShift)
//                | ...)
//
//      so a lookup keyed on attributes==0 can be answered with a transition whose target marks the slot RAW.
//
//   2. The descriptor structure is built LAZILY, off a base Structure that script can reach.
//      ObjectConstructor.h:95-99:
//
//        Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(
//            &globalObject, globalObject.objectPrototype(), JSFinalObject::defaultInlineCapacity);
//        PropertyOffset offset;
//        structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->value, 0, offset);
//
//      and JSGlobalObject.cpp initLaters m_dataPropertyDescriptorObjectStructure /
//      m_accessorPropertyDescriptorObjectStructure. Laziness is load-bearing: `Object.create(Object.prototype)`
//      hands script the SAME Structure, so `o.value = 1.5` installs the raw {value} transition into that
//      Structure's table BEFORE the descriptor builder ever asks for one. The builder's attributes-0 lookup
//      then gets handed the script's raw transition.
//
//   3. The builder stores a non-number into it. ObjectConstructor.h:133:
//
//        result->putDirectOffset(vm, dataPropertyDescriptorValuePropertyOffset, descriptor.value());
//
//      -> JSObject::putDirectOffsetRawDoubleAware -> the fail-closed arm at JSObject.cpp:405-411.
//
//   The RELEASE_ASSERT is the patch working as designed — it is deliberately there so this class of hole
//   fails closed instead of mis-typing a pointer. The bug is that the hole exists: nothing widens the
//   representation on this path, because the engine-internal builder never goes through putDirectInternal.
//
// DISCRIMINATORS (all clean, i.e. they isolate the cause to exactly this mechanism)
//   o.value = 1            -- Int32 creating store, no raw transition created       -> PASS
//   var o = {}             -- literal, a DIFFERENT base structure                   -> PASS
//   descriptor of {x:3.25} -- the descriptor's own value is numeric                 -> PASS
//   descriptor taken BEFORE the poisoning store                                     -> PASS
//
// GENERALITY
//   The same shape applies to the ACCESSOR descriptor structure (`o.get = 1.5`, then take a descriptor of a
//   getter/setter pair) and, by construction, to any other lazily-built engine structure whose property
//   names a script can also add to the shared empty-object structure — iterator results (`value`, `done`),
//   promise reaction records, and so on. Only the two descriptor structures are exercised below.
//
// FIX SHAPE
//   Engine-internal structure builders must not accept a representation-carrying transition. Either build
//   them off a private base structure that script cannot transition, or normalize the representation bit
//   out of the target the way Structure::normalizeRepresentationAttributes already does for accessors —
//   i.e. addPropertyTransition should be able to demand a BOXED slot and get a sibling structure if the
//   cached one is raw. The deeper issue is that masking the representation bit out of the transition key
//   makes "give me the transition for attributes A" an ambiguous request; every caller that is not
//   prepared to widen needs a way to say "boxed, please".

let failures = 0;

// ---- 1. the data-property descriptor structure
{
    const o = Object.create(Object.prototype);
    o.value = 1.5;                                   // poisons the shared empty-object structure's {value} transition
    const d = Object.getOwnPropertyDescriptor({ x: "a string" }, "x");
    if (d.value !== "a string" || typeof d.value !== "string") {
        failures++;
        print("FAIL: descriptor .value = " + d.value + " (typeof " + typeof d.value + "), expected the string");
    }
}

// ---- 2. the accessor-property descriptor structure
{
    const o = Object.create(Object.prototype);
    o.get = 1.5;
    const src = { get x() { return 1; }, set x(v) { } };
    const d = Object.getOwnPropertyDescriptor(src, "x");
    if (typeof d.get !== "function") {
        failures++;
        print("FAIL: accessor descriptor .get is " + typeof d.get + ", expected function");
    }
}

if (failures)
    throw new Error(failures + " failure(s)");
print("PASS");
