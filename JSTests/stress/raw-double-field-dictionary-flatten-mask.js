//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//
// Regression test. Reduction, measurement log and the full mechanism write-up live in
// JS-data analysis/prompt/box2d/repro/bugs/06-repro-dictionary-flatten-stale-mask.js and the fix design in that directory's fixes/.
//
// REPRO for bug 06 — Structure::flattenDictionaryStructure COMPACTS PROPERTY OFFSETS but never renumbers
// the per-offset raw-double mask, and copies the slot values out and back with a raw-unaware read/write
// pair. After a flatten the mask describes the WRONG slots.
//
// Run (WebKit-security/OpenSource):
//   ./Tools/Scripts/run-jsc --release analysis/prompt/box2d/repro/bugs/06-repro-dictionary-flatten-stale-mask.js
//
//   --useRawDoubleFieldStorage=1  (the staged DEFAULT) -> detector fires inside renumberPropertyOffsets
//   --useRawDoubleFieldStorage=0                       -> "PASS"
//   pre-patch build (WebKitBuild-without/Release)      -> "PASS"
//
// OBSERVED (release-with-assertions build of the staged tree, rc=137):
//
//   ASSERTION FAILED: unchecked getDirect(PropertyOffset) read a raw-double field at offset 1; ...
//   3  JSC::PropertyTable::renumberPropertyOffsets(JSC::JSObject*, unsigned int, WTF::Vector<JSC::JSValue,...>&)
//   4  JSC::Structure::flattenDictionaryStructure(...)
//
// MECHANISM — two defects at one site, neither of which the patch touches.
//
//   Structure.cpp:1114-1125, the isUncacheableDictionary() arm of flattenDictionaryStructure:
//
//       Vector<JSValue> values(propertyCount);
//       PropertyOffset offset = table->renumberPropertyOffsets(object, m_inlineCapacity, values);
//       setMaxOffset(vm, offset);
//       for (unsigned i = 0; i < propertyCount; i++)
//           object->putDirectOffset(vm, offsetForPropertyNumber(i, m_inlineCapacity), values[i]);
//
//   and PropertyTable.cpp:213-226:
//
//       forEachPropertyMutable([&](auto& entry) {
//           values[i] = object->getDirect(entry.offset());     // <-- (a) UNCHECKED read
//           offset = offsetForPropertyNumber(i, inlineCapacity);
//           entry.setOffset(offset);                           // <-- (b) offset MOVES
//           ++i;
//       });
//
//   (a) The read is the unchecked getDirect(PropertyOffset), so a raw slot yields bits(d) as a JSValue.
//       The write-back then goes through the 3-arg putDirectOffset, which forwards to
//       putDirectOffsetRawDoubleAware against the (already renumbered) structure — so the corrupted value
//       is re-encoded by a raw-aware writer consulting a stale mask.
//
//   (b) entry.setOffset() moves the property, but nothing renumbers m_rawDoubleMask. The only mutators of
//       the mask are Structure::add / remove / attributeChange (StructureInlines.h) and the two transition
//       functions in Structure.cpp, plus copyRawDoubleMaskFrom which is called only from finishCreation.
//       flattenDictionaryStructure mutates `this` in place and returns `this`, so none of them run.
//       Neither PropertyTable.cpp nor flattenDictionaryStructure appears in the staged diff at all.
//
//   The result is a mask that claims offsets it no longer owns (OVER-claim, the crash direction the patch's
//   own documentation calls out) and fails to claim ones it does (UNDER-claim, a raw slot read as a
//   JSValue). Which of the two you get depends on how far the compaction shifted each property.
//
//   NOTE ON A NEGATIVE RESULT. Simple flatten probes do NOT show this. A dictionary whose surviving
//   properties are ALL raw doubles renumbers within an all-raw set, so the shifted mask still happens to
//   answer correctly; and a small object whose offsets are already dense does not shift at all. Two of my
//   own earlier probes (20 and 24 mixed-representation properties, flattened via $vm.flattenDictionaryObject)
//   came back clean for exactly that reason. The trigger below is what actually moves a raw offset: a
//   DELETE IN FRONT of the raw property, on an object wide enough to have become an uncacheable dictionary,
//   with the flatten driven by the real repatch path rather than by $vm.
//
// TRIGGER
//   * `delete o.f` where f precedes the raw double `d` and the cell `c` -> uncacheable dictionary, and the
//     survivors all shift DOWN.
//   * 200 extra properties so the structure is genuinely a dictionary.
//   * A hot `x.p7` read, so Repatch -> flattenDictionaryObject runs the flatten. Calling
//     $vm.flattenDictionaryObject directly also works but is less representative.
//
// IMPACT
//   Assertion crash on any assertion-enabled build. On a plain-release build the value corruption is silent,
//   and a mask that has shifted onto a slot holding a cell puts that cell on the collector's skip list —
//   the same use-after-free shape as bug 05. NOT DEMONSTRATED: no assertions-off build was available in
//   this session, so only the detector firing is observed here.
//
// FIX SHAPE
//   flattenDictionaryStructure has to rebuild the mask from the renumbered property table, in the same
//   critical section that renumbers it — walk the table after renumberPropertyOffsets and re-derive every
//   bit from attributesSayDoubleRepresentation(entry.attributes()). And renumberPropertyOffsets must read
//   with getDirect(Structure&, PropertyOffset) using the PRE-renumber structure, then the write-back must
//   use the POST-renumber one. Structure::validateRawDoubleMaskAgreement already exists and should be
//   called at the end of flattenDictionaryStructure, where it would have caught this.

var src = "var o = {}; o.f = 1; o.d = 1.5; o.c = { tag: 12345 };";
for (var i = 0; i < 200; ++i)
    src += "o.p" + i + " = " + i + ";";
src += "delete o.f; return o;";          // delete IN FRONT of d and c -> uncacheable dictionary + shift
var build = new Function(src);

var a = [];
for (var i = 0; i < 500; ++i)
    a.push(build());

function read(x) { return x.p7; }
for (var i = 0; i < 500; ++i)            // drives Repatch -> flattenDictionaryObject
    read(a[i]);

var failures = 0;
for (var i = 0; i < a.length; ++i) {
    if (a[i].d !== 1.5) { failures++; if (failures === 1) print("FAIL: a[" + i + "].d = " + a[i].d + ", expected 1.5"); }
    if (!a[i].c || a[i].c.tag !== 12345) { failures++; if (failures === 1) print("FAIL: a[" + i + "].c = " + a[i].c); }
}

if (failures)
    throw new Error(failures + " failure(s) after dictionary flatten");
print("PASS");
