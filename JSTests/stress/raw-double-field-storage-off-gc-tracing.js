//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=0")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/05-repro-gc-option-gating-uaf.js and the fix design in that directory's fixes/.
//
// REPRO for bug 05 — USE-AFTER-FREE IN THE FEATURE-OFF LEG. With `--useRawDoubleFieldStorage=0` the
// collector still skips slots the raw-double mask claims, but the widening that keeps cells out of those
// slots is gated on that very option. So a live JSObject stored in a mask-claimed slot is never traced,
// is swept, and its storage is handed to the next allocation.
//
// THIS IS THE CONTROL LEG. Every A/B measurement in this project uses --useRawDoubleFieldStorage=0 as the
// "clean" side, and it is not clean.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release --useRawDoubleFieldStorage=0 \
//       analysis/prompt/box2d/repro/bugs/05-repro-gc-option-gating-uaf.js
//
//   --useRawDoubleFieldStorage=0                       -> bad=20000/20000, rc=0, SILENT. FAIL
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> bad=0/20000, "PASS"
//   --useDoubleFieldRepresentation=0                   -> "PASS" (nothing is ever marked)
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// Deterministic: 12/12 identical runs, no assertion on any build, rc=0. Silent heap corruption.
//
// MECHANISM — two predicates that were supposed to be the same question.
//
//   Structure::isRawDoubleOffset(offset)                     // StructureInlines.h -- NOT option-gated
//   Structure::slotHoldsRawDouble(offset)
//       { return Options::useRawDoubleFieldStorage() && isRawDoubleOffset(offset); }   // gated
//
//   The COLLECTOR uses the ungated one. JSObject.cpp appendNonRawDoubleValues:
//       if (!structure->isRawDoubleOffset(offset)) [[likely]]
//           continue;                                        // ... otherwise the slot is skipped
//   reached from markAuxiliaryAndVisitOutOfLineProperties and JSFinalObject::visitChildrenImpl, both gated
//   only on structure->hasRawDoubleFields(), which is driven by useDoubleFieldRepresentation (default TRUE).
//
//   The WIDENING GUARD uses the gated one. JSObjectInlines.h putDirectInternal:
//       if (Options::useRawDoubleFieldStorage() && !value.isDouble() && !value.isInt32()) [[unlikely]] {
//           if (widenDoubleRepresentation(vm, propertyName))
//               slot.disableCaching();
//       }
//   and the add-path sibling at JSObjectInlines.h:637-673 is likewise inside a useRawDoubleFieldStorage
//   check.
//
//   So with storage=0 and marking=1:
//     * `o.a = 1.5` marks {}, a as RepresentationDouble and SETS the mask bit;
//     * a later `o.a = someObject` on the same shape does NOT widen, because widening is switched off;
//     * the slot now holds a genuine, correctly boxed JSCell*, and the mask still says "raw";
//     * the collector consults the mask and skips it.
//
//   Confirmed directly, with no GC involved:
//       o.a = 1.5;                 $vm.isRawDoubleField(o,"a") -> true
//       o.a = { tag: "a cell" };   $vm.isRawDoubleField(o,"a") -> true      // storage=0
//                                  $vm.isRawDoubleField(o,"a") -> false     // storage=1, widening fired
//
// IMPACT
//   Use-after-free of a reachable JSObject, escalating to controlled type confusion. The subagent that
//   found this also showed the freed cell being reallocated into a script-chosen shape, so that a stale
//   reference the script still legitimately holds resolves to an object whose field names and values the
//   script picked. Reproduced here as 20000/20000 corrupted references; the shape-confusion escalation is
//   in that agent's transcript and is NOT re-demonstrated by this file.
//
// FIX SHAPE
//   The GC must ask the same question the writers ask. Either make appendNonRawDoubleValues use
//   slotHoldsRawDouble(), or make Structure::hasRawDoubleFields()/the mask itself never be populated when
//   useRawDoubleFieldStorage is off. The second is preferable: it removes the whole class of
//   "marking is on but storage is off" divergences rather than fixing one consumer, and it also disarms
//   the ablation options that this project relies on for measurement.
//
//   Note that the same asymmetry makes every ablation in 07-PLAN suspect: a measurement taken with
//   storage=0 was taken on a build that can silently free live objects.

function makeD() { var o = {}; o.a = 1.5; return o; }                    // creating store is a double -> marked, mask set
function makeC(i) { var o = {}; o.a = { tag: 12345, i: i }; return o; }  // same masked transition key, value is a CELL

makeD();                                   // establish the RepresentationDouble transition for {} -> {a}

var keep = [];
for (var i = 0; i < 20000; ++i)
    keep.push(makeC(i));                   // the ONLY strong reference to each inner object is a raw-claimed slot

for (var i = 0; i < 4; ++i) gc();
for (var i = 0; i < 300000; ++i) { var t = { tag: 999, i: -1 }; }   // same-shape churn reuses the swept cells
for (var i = 0; i < 2; ++i) gc();

var bad = 0;
for (var i = 0; i < keep.length; ++i) {
    var v = keep[i].a;
    if (!v || v.tag !== 12345 || v.i !== i)
        bad++;
}

print("keep[0].a = " + JSON.stringify(keep[0].a) + "   (expected {\"tag\":12345,\"i\":0})");
if (bad) {
    print("FAIL: " + bad + "/" + keep.length + " reachable objects were collected and their storage reused");
    throw new Error("use-after-free: " + bad + "/" + keep.length);
}
print("PASS");
