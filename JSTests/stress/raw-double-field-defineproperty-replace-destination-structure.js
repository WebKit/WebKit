//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/02-repro-defineproperty-attrchange-addrof.js and the fix design in that directory's fixes/.
//
// REPRO for bug 02 — `Object.defineProperty` on an existing property stores through the OLD structure,
// so the value lands NaN-BOXED in a slot the newly installed structure claims is a RAW double.
// Every raw-aware reader then de-biases it. When the stored value is a CELL this hands the script the
// object's heap address as a Number: a complete `addrof` primitive, in plain JS, at every tier.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/02-repro-defineproperty-attrchange-addrof.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> prints a heap address, FAIL
//   --useRawDoubleFieldStorage=0                       -> prints "PASS"
//   pre-patch build (WebKitBuild-without/Release)      -> prints "PASS"
//
// Reproduces identically with --useJIT=0, --useDFGJIT=0 and full JIT. No warmup, no OSR, no impure NaN,
// no $vm. This is the C++ property path only.
//
// MECHANISM
//   JSObjectInlines.h putDirectInternal, the "property already exists" arm (JSObjectInlines.h:682-706):
//
//       unsigned currentAttributes;
//       PropertyOffset offset = structure->get(vm, propertyName, currentAttributes);
//       if (offset != invalidOffset) {
//           ...
//           putDirectOffset(vm, offset, value);                       // <-- line 689, BARE overload
//           if (mode == PutModeDefineOwnProperty && userVisibleAttributes(newAttributes) != ...) {
//               setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName,
//                                                                     newAttributes, &deferred));
//           }
//
//   The bare `putDirectOffset(vm, offset, value)` resolves its destination as `*this->structure()` —
//   which at that instant is still the PRE-transition structure. The attribute-change transition installed
//   on the next line is a different Structure, and it can claim the slot is a raw double. This is the exact
//   "asked the wrong authority, stored before setStructure" defect the patch already fixed in four other
//   places (operationReallocateButterflyAndTransition, operationPutByMegamorphicReallocating,
//   putOwnDataPropertyBatching, and putDirectInternal's own ADD path at JSObjectInlines.h:733). The replace
//   path was missed.
//
//   Two things make the new structure claim raw:
//
//   (1) `newAttributes` picks up PropertyAttribute::RepresentationDouble whenever the incoming value is a
//       double (JSObjectInlines.h:549). So redefining a boxed slot with a double produces a raw-claiming
//       target while the store already went out boxed. That alone is the wrong-value half: 2.5 reads back
//       as 2.75, i.e. exactly +DoubleEncodeOffset (2^49) in the mantissa.
//
//   (2) That attribute-change transition is then CACHED, and StructureTransitionTable::Hash::Key
//       deliberately masks RepresentationDouble out of the key. So a later redefinition of the same shape
//       with the same user-visible attributes but a NON-number value finds the same raw-claiming structure.
//       putDirectInternal's widening guard cannot help: JSObject::widenDoubleRepresentation ran earlier
//       against the CURRENT structure, where the property is not (yet) raw, so it returns false.
//
//   The cell therefore reaches a slot whose Structure says "raw IEEE-754 bits", and
//   JSObject::getDirectRawDoubleAware hands the pointer back to JavaScript as jsDoubleNumber(bits).
//
//   Note that putDirectOffsetRawDoubleAware() already RELEASE_ASSERTs on a non-numeric store into a raw
//   slot precisely to make this fail closed — but the bare overload never reaches it, because it asked the
//   old structure and took the plain locationForOffset()->set() path.
//
// IMPACT
//   * addrof: a script-chosen JSCell's address, exact, as a Number. Demonstrated below on JSFinalObject and
//     JSString; consecutive allocations come back 32 bytes apart.
//   * type confusion: `typeof o.a` is "number" for a slot holding an object pointer, and
//     `$vm.isRawDoubleField(o, "a")` returns TRUE for that same slot -- i.e. the mask OVER-CLAIMS, which is
//     the direction the patch's own documentation calls "the crash".
//   * USE-AFTER-FREE: tracing consults the SAME mask (JSObject.cpp appendNonRawDoubleValues, gated on
//     Structure::hasRawDoubleFields and then per-offset), so a live cell parked in such a slot is on the
//     collector's skip list and is swept while still referenced. DEMONSTRATED, deterministic 5/5 --
//     see 02b-repro-defineproperty-attrchange-uaf.js, which is a separate file because its liveness oracle
//     is an address hunt and needs an unperturbed allocation sequence.
//
//     CORRECTION: this file previously said the use-after-free was NOT demonstrated. That was based on two
//     failed probes of my own -- a FinalizationRegistry test whose own positive control also reported zero
//     (so the instrument was dead, and the run proved nothing), and a zombie-mode run that never
//     dereferenced the leaked pointers. The working oracle is bug 02's own addrof: if a LATER freshly
//     allocated object reports the victim's address, the victim's cell was freed and reused. It has a
//     negative control that stays clean over 200000 allocations.
//
// SCOPE — measured, not assumed. Every variant below leaks a pointer under storage=1 and passes under
// storage=0. They are all the REPLACE arm; the ADD paths are correctly guarded by the widening code at
// JSObjectInlines.h:637-673 and were verified clean.
//
//   LEAKS                                                    | ADD PATHS THAT ARE FINE
//   -------------------------------------------------------- | --------------------------------------
//   Object.defineProperty on an existing property             | Object.defineProperty adding a property
//   Reflect.defineProperty on an existing property            | Object.defineProperties adding
//   accessor -> data redefinition                             | Object.create(proto, descriptors)
//   { writable: false } redefinition                          | plain `o.a = victim` assignment
//   redefinition on a DICTIONARY object (the sibling arm at
//     JSObjectInlines.h:604 has the same store-then-transition shape)
//   redefinition of a class field
//   redefinition on a .prototype object (poisons every instance)
//
// FIX SHAPE
//   Pass the destination structure at JSObjectInlines.h:689, as the sibling ADD path at :733 already does —
//   i.e. compute the attribute-change transition first, then
//   `putDirectOffset(vm, *newStructure, offset, value)`. Compare the identical corrections at
//   JITOperations.cpp operationReallocateButterflyAndTransition and JSObject.cpp
//   putOwnDataPropertyBatching. The dictionary arm at JSObjectInlines.h:604 needs the same treatment —
//   it is confirmed to leak by variant E of the scope table above.
//
//   A fix must also decide what the transition target should be when the value is NOT a number: today the
//   masked transition key can hand back a raw-claiming sibling, and the widening guard cannot fire because
//   widenDoubleRepresentation() was already consulted against the pre-transition structure. Simply passing
//   the right structure turns the leak into the RELEASE_ASSERT inside putDirectOffsetRawDoubleAware
//   ("store of a non-numeric value into a Double-represented field"), i.e. a crash rather than a
//   disclosure — better, but still a denial of service that plain JS can reach. The representation has to
//   be widened on this path too.

const f64 = new Float64Array(1);
const u32 = new Uint32Array(f64.buffer);
function bits(v) {
    if (typeof v !== "number")
        return "(" + typeof v + ")";
    f64[0] = v;
    return "0x" + (u32[1] >>> 0).toString(16).padStart(8, "0") + (u32[0] >>> 0).toString(16).padStart(8, "0");
}

let failures = 0;
function fail(msg) { failures++; print("FAIL: " + msg); }

// ---- (a) WRONG VALUE, on its own. The creating store is an Int32 so the slot is boxed; the redefinition
//          value is a double so the transition target claims raw; the store already went out boxed.
{
    const o = {};
    o.a = 5;
    Object.defineProperty(o, "a", { value: 2.5, enumerable: false });
    if (o.a !== 2.5)
        fail("redefined o.a reads " + o.a + " " + bits(o.a) + ", expected 2.5 " + bits(2.5)
             + "  (delta is exactly DoubleEncodeOffset)");
}

// ---- (b) ADDROF. The transition cached by (a) is keyed WITHOUT the representation bit, so this second
//          object with a cell value lands on the same raw-claiming structure.
function addrof(victim) {
    const o = {};
    o.a = 5;
    Object.defineProperty(o, "a", { value: victim, enumerable: false });
    return o.a;
}

const v1 = { tag: 1 };
const v2 = { tag: 2 };
const str = "a string";

const a1 = addrof(v1);
const a2 = addrof(v2);
const a3 = addrof(str);

for (const [name, leaked, want] of [["object v1", a1, "object"], ["object v2", a2, "object"], ["string", a3, "string"]]) {
    if (typeof leaked !== want)
        fail("addrof(" + name + ") returned typeof " + typeof leaked + " = " + bits(leaked)
             + "  -- that is the cell pointer, disclosed as a Number");
}

if (typeof a1 === "number" && typeof a2 === "number") {
    f64[0] = a1; const lo1 = u32[0] >>> 0;
    f64[0] = a2; const lo2 = u32[0] >>> 0;
    print("  two consecutively allocated objects are " + (lo2 - lo1) + " bytes apart -> real heap addresses");
}

if (!failures)
    print("PASS");
else
    throw new Error(failures + " failure(s)");
