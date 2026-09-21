//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/03-repro-private-field-unchecked-read.js and the fix design in that directory's fixes/.
//
// REPRO for bug 03 — PRIVATE CLASS FIELDS are an unhooked raw-double reader.
// JSObject::getPrivateFieldSlot holds the Structure and still uses the UNCHECKED
// getDirect(PropertyOffset), so a `#field` whose creating store was a double is read as a JSValue
// straight out of a slot that holds bare IEEE-754 bits.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/03-repro-private-field-unchecked-read.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT), assertions ON  -> the patch's own detector fires
//                                                                        and the process dies before any output
//   --useRawDoubleFieldStorage=0                                       -> prints "PASS"
//   pre-patch build (WebKitBuild-without/Release)                      -> prints "PASS"
//
// Reproduces with --useJIT=0 as well: the read never leaves C++, so no tier-up is involved.
//
// OBSERVED (release-with-assertions build of the staged tree, exit 137, nothing printed):
//
//   ASSERTION FAILED: unchecked getDirect(PropertyOffset) read a raw-double field at offset 0;
//                     this caller needs the Structure-aware getDirect(Structure&, PropertyOffset)
//   !structure->isRawDoubleOffset(offset)
//   .../runtime/JSObject.cpp(280) : void JSC::JSObject::assertNotRawDoubleFieldRead(PropertyOffset) const
//   2  JSC::JSObject::assertNotRawDoubleFieldRead(int) const
//   3  JSC::JSObject::getPrivateField(JSC::JSGlobalObject*, JSC::PropertyName, JSC::PropertySlot&)
//   4  llint_slow_path_get_private_name
//
// MECHANISM
//   JSObjectInlines.h:974-1001, JSObject::getPrivateFieldSlot:
//
//       Structure* structure = object->structure();              // <-- the Structure IS in hand
//       unsigned attributes;
//       PropertyOffset offset = structure->get(vm, propertyName, attributes);
//       if (offset == invalidOffset)
//           return false;
//       JSValue value = object->getDirect(offset);               // <-- line 985, UNCHECKED overload
//
//   Every other C++ reader in this patch was converted to getDirect(Structure&, PropertyOffset); this one
//   was not. It is not covered by the B15 sweep either, because nothing in JSTests/stress puts a double
//   initializer on a private field.
//
//   That the slot really is raw is confirmed by the patch's own instrumentation on this exact program
//   (--dumpRawDoubleCorruption=1 --dumpDoubleFieldSplitCensus=1):
//
//       [rawdouble] MARK prop=#p class=Object
//       [rawdouble] setRawDoubleOffset SET offset=0 structure=0x301006d30 maxOffset=0 nowReadsRaw=true
//
//   so JSObject::putDirectOffsetRawDoubleAware took its memcpy-of-bit_cast path and the slot holds bits(d).
//
// WHAT A PLAIN-RELEASE BUILD DOES  -- DERIVED FROM THE CODE, NOT DEMONSTRATED.
//   assertNotRawDoubleFieldRead is `#if ASSERT_ENABLED`, so with assertions off the read is just
//   `locationForOffset(offset)->get()` and the raw word is handed to JavaScript as a JSValue:
//
//       #p = 1.5     bits 0x3ff8000000000000  -> a Number, but bits-2^49 == 1.375     (wrong value)
//       #p = 0.0     bits 0x0000000000000000  -> the EMPTY JSValue reaches JS         (null deref)
//       #p = 5e-324  bits 0x0000000000000001  -> JSValue::isCell() is TRUE, pointer 0x1  (type confusion)
//
//   The double is fully script-controlled, so for any bit pattern below 2^48 the "cell" pointer is chosen
//   by the script -- the same primitive as bug 01, but reachable from the interpreter on first execution
//   with no impure NaN and no DFG. NOTE: no assertions-off build was available in this session, so the
//   three lines above are read off JSValue's encoding and NOT observed. Build one and re-run before
//   quoting them.
//
//   Note also that the `#if ASSERT_ENABLED` block immediately below the bad read in getPrivateFieldSlot
//   does `value.asCell()->type()` on any cell-looking value -- so on an assertion-enabled build the
//   subnormal case would segfault there too, if the earlier detector did not fire first.
//
// TRIGGER SHAPE
//   The CREATING store must be a double, which for a private field means a double FIELD INITIALIZER.
//     class C { #p = 1.5; }                 -> marked, reproduces
//     class C { #p; constructor(v){this.#p=v;} }  with v=1.5
//                                           -> NOT marked: the creating store is `undefined`, so the slot
//                                              stays boxed and the replace never revises the representation
//
// FIX SHAPE
//   One character class of change at JSObjectInlines.h:985 --
//       JSValue value = object->getDirect(*structure, offset);
//   `structure` is already the local on the line above, so this is the same edit already applied to
//   getOwnNonIndexPropertySlot, getDirectConcurrently, JSONObject, ObjectConstructor and the rest.
//
// SEE ALSO — other unchecked getDirect(PropertyOffset) call sites still in the tree that a Structure-
//   carrying caller reaches:
//
//     runtime/JSObject.cpp:955  JSObject::analyzeHeap  -- CONFIRMED REACHABLE. The detector fires on
//        `for (i=0;i<50;i++){const o={};o.a=1.5;o.b=5e-324;o.c=0.0;keep.push(o);} generateHeapSnapshotForGCDebugging()`
//        under --useRawDoubleFieldStorage=1, and not with it off. Impact is limited though: the only
//        in-tree HeapAnalyzer, HeapSnapshotBuilder::analyzePropertyNameEdge, just appends the pointer and
//        json() looks it up in a hash map, dropping unknown targets -- so this corrupts snapshot data
//        (bogus edges, doubles misreported) rather than dereferencing the fake cell. Still needs the same
//        one-line fix; `structure` is on the line above.
//
//     runtime/PropertyTable.cpp:219  PropertyTable::renumberPropertyOffsets, called from
//        Structure::flattenDictionaryStructure -- reads every value unchecked AND renumbers offsets while
//        the per-offset raw-double mask is not renumbered with them. UNCONFIRMED: probes over 20- and
//        24-property mixed-representation uncacheable dictionaries, flattened via
//        $vm.flattenDictionaryObject, showed no wrong values and a still-correct mask. Needs a case where
//        compaction actually moves a raw offset onto a boxed one.
//
//     bytecode/ObjectPropertyConditionSet.cpp:438 -- not exercised.

class Box {
    #p = 1.5;                       // double initializer -> the creating store marks the slot raw
    read() { return this.#p; }
}

class Sub {
    #q = 5e-324;                    // raw bits 0x1 -- passes JSValue::isCell() when read unchecked
    read() { return this.#q; }
}

class Zero {
    #z = 0.0;                       // raw bits 0x0 -- the EMPTY JSValue when read unchecked
    read() { return this.#z; }
}

let failures = 0;
function check(what, actual, expected) {
    if (!Object.is(actual, expected)) {
        failures++;
        print("FAIL: " + what + " read back " + actual + " (typeof " + typeof actual + "), expected " + expected);
    }
}

check("#p = 1.5", new Box().read(), 1.5);
check("#q = 5e-324", new Sub().read(), 5e-324);
check("#z = 0.0", new Zero().read(), 0.0);

// A private METHOD alongside a private field reaches the same slow path from a different opcode.
class WithMethod {
    #v = 2.25;
    #m() { return 7; }
    read() { return this.#m() + this.#v; }
}
check("private method + field", new WithMethod().read(), 9.25);

if (!failures)
    print("PASS");
else
    throw new Error(failures + " failure(s)");
