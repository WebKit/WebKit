//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/04-repro-proxy-handler-trap-fake-cell.js and the fix design in that directory's fixes/.
//
// REPRO for bug 04 — a Proxy HANDLER TRAP read through ProxyObject's own offset cache uses the unchecked
// getDirect(PropertyOffset). A raw-double handler property therefore becomes a script-chosen JSCell*, and
// getCallDataInline dereferences it immediately.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/04-repro-proxy-handler-trap-fake-cell.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> the detector fires on the SECOND trap lookup
//   --useRawDoubleFieldStorage=0                       -> three clean TypeErrors, "PASS"
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// Reproduces with --useJIT=0. Pure C++ runtime path, no tier-up, no warmup, no $vm.
//
// OBSERVED (release-with-assertions build of the staged tree):
//
//   it 0: TypeError: 'get' property of a Proxy's handler should be callable
//   ASSERTION FAILED: unchecked getDirect(PropertyOffset) read a raw-double field at offset 0; ...
//   2  JSC::JSObject::assertNotRawDoubleFieldRead(int) const
//   3  JSC::JSObject::getDirect(this=0x115144180, offset=<unavailable>) const at JSObject.h:511
//   4  JSC::ProxyObject::getHandlerTrap(...) at ProxyObject.cpp:116
//
//   Iteration 0 goes through the correct raw-aware slot path and throws the expected TypeError, but it
//   POPULATES the cache on the way out. Iteration 1 takes the cached fast path and reads the slot raw.
//
// MECHANISM
//   ProxyObject::getHandlerTrap (ProxyObject.cpp:93-143) keeps its own per-trap offset cache, separate from
//   every inline-cache mechanism the patch audited:
//
//       if (isHandlerTrapsCacheValid(handler)) {
//           PropertyOffset offset = m_handlerTrapsOffsetsCache[static_cast<uint8_t>(trap)];
//           if (offset == invalidOffset)
//               return nullptr;
//           if (offset != emptyHandlerTrapCache)
//               return ensureIsCallable(handler->getDirect(offset));      // <-- ProxyObject.cpp:116
//       }
//
//   `handler->getDirect(offset)` is the unchecked overload (JSObject.h:508). ProxyObject has the Structure
//   in hand — isHandlerTrapsCacheValid just compared handler->structureID() against the cached one — so the
//   raw-aware getDirect(Structure&, PropertyOffset) was available and simply was not used.
//
//   The cache is populated BEFORE the callability check (ProxyObject.cpp:123-135 set the offsets and the
//   structure IDs; ensureIsCallable is not until :141), so a NON-CALLABLE trap value still installs the
//   cache entry. That is what makes the second lookup reachable at all.
//
//   ProxyObject.cpp is not touched by the staged diff. It is an unmodified file whose meaning the patch
//   changed by redefining what getDirect(PropertyOffset) returns — which is exactly why the audit missed it.
//   The same fast path serves every trap (get, set, has, deleteProperty, ownKeys, ...), so the surface is
//   wider than the `get` case below.
//
// WHAT A PLAIN-RELEASE BUILD DOES  -- DERIVED FROM THE CODE, NOT DEMONSTRATED HERE.
//   assertNotRawDoubleFieldRead is #if ASSERT_ENABLED. With assertions off the read returns the raw word,
//   and the consumer is not benign:
//
//       auto ensureIsCallable = [&](JSValue value) -> JSObject* {
//           if (value.isUndefinedOrNull()) return nullptr;
//           callData = JSC::getCallDataInline(value);        // ProxyObject.cpp:101
//
//   getCallDataInline(JSValue) is `if (!value.isCell()) return {}; return getCallDataInline(value.asCell());`
//   and the cell overload's first statement is `cell->type()` — an unconditional load at ptr+5.
//   isCell() is !(bits & (NumberTag|OtherTag)), so any double whose IEEE-754 pattern is below 2^48 with bit 1
//   clear is classified as a cell whose pointer IS that pattern. PAYLOAD below is written through a
//   BigUint64Array/Float64Array union, so the whole 48-bit user address space is script-reachable.
//
//   NO ASSERTIONS-OFF BUILD WAS AVAILABLE IN THIS SESSION, so the fault at the chosen address was not
//   observed directly here. Build one and re-run before quoting a faulting address.
//
// FIX SHAPE
//   ProxyObject.cpp:116 -> `handler->getDirect(*handler->structure(), offset)`. Alternatively, refuse to
//   populate m_handlerTrapsOffsetsCache when structure->slotHoldsRawDouble(offset), which is the shape the
//   patch already uses for the megamorphic cache (JITOperations.cpp megamorphicCacheWouldExposeRawDouble)
//   and for the LLInt/baseline caches (Structure::inlineCachesCanAccessSlotDirectly).

const buf = new ArrayBuffer(8);
const f64 = new Float64Array(buf);
const b64 = new BigUint64Array(buf);

// bit 1 clear and < 2^48, so JSValue::isCell() is TRUE and the pointer is exactly this constant.
const PAYLOAD = 0x0000414141414140n;
b64[0] = PAYLOAD;

const handler = {};
handler.get = f64[0];            // the creating store is a double -> the slot is marked and stored RAW

const p = new Proxy({}, handler);

let typeErrors = 0;
let failures = 0;
for (let i = 0; i < 3; ++i) {
    try {
        const v = p.someProp;
        failures++;
        print("FAIL: iteration " + i + " returned " + v + " instead of throwing");
    } catch (e) {
        if (e instanceof TypeError)
            typeErrors++;
        else {
            failures++;
            print("FAIL: iteration " + i + " threw " + e);
        }
    }
}

// Correct behaviour: the handler's `get` is a Number, so every lookup rejects it the same way.
if (typeErrors !== 3) {
    failures++;
    print("FAIL: expected 3 TypeErrors, got " + typeErrors);
}

// A minimal variant with no typed arrays at all, in case the ArrayBuffer aliasing above is ever the thing
// that changes: 5e-324 has raw bits 0x1, which is equally cell-shaped.
{
    const h2 = {};
    h2.get = 5e-324;
    const p2 = new Proxy({}, h2);
    let t2 = 0;
    for (let i = 0; i < 3; ++i) {
        try { p2.someProp; } catch (e) { if (e instanceof TypeError) t2++; }
    }
    if (t2 !== 3) {
        failures++;
        print("FAIL: 5e-324 variant produced " + t2 + " TypeErrors, expected 3");
    }
}

if (!failures)
    print("PASS");
else
    throw new Error(failures + " failure(s)");
