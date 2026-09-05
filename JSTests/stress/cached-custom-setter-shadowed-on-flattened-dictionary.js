//@ runDefault("--useConcurrentJIT=false")

// Companion to cached-custom-accessor-shadowed-on-flattened-dictionary.js, for
// the put path. A custom *setter* served from a static property table is cached
// by the JIT as a constant setter call guarded only by a structure check. On a
// dictionary that has already been flattened once (as a long-lived global
// object / DOM window becomes), the property can be shadowed by a plain data
// property in place -- no structure transition -- which must stop the custom
// setter from being invoked. A stale cache would keep calling it.

function assert(b, msg) {
    if (!b)
        throw new Error("FAILED: " + (msg || ""));
}

// testStaticValueSetFlag's custom setter sets testStaticValueSetterCalled=true
// as a side effect and does NOT write the property itself, so the property
// stays a custom during warmup and the put inline cache caches the custom setter.
let o = $vm.createStaticCustomValue();

function put(obj, v) { obj.testStaticValueSetFlag = v; }
noInline(put);

// 1) cacheable dictionary, warm up to flatten (sets hasBeenFlattenedBefore).
o = $vm.toCacheableDictionary(o);
for (let i = 0; i < testLoopCount; ++i)
    put(o, i);
// 2) re-convert to a cacheable dictionary; the new structure inherits
//    hasBeenFlattenedBefore, so prepareChainForCaching will not re-flatten it.
o = $vm.toCacheableDictionary(o);
// 3) warm up again so the custom setter is (re)cached against this dictionary.
for (let i = 0; i < testLoopCount; ++i)
    put(o, i);
assert(o.testStaticValueSetterCalled === true, "sanity: custom setter ran during warmup");

// 4) Shadow the custom with a plain data property. On a dictionary this is an
//    in-place add with NO structure transition (same StructureID).
Object.defineProperty(o, "testStaticValueSetFlag", { value: -1, writable: true, configurable: true, enumerable: true });
o.testStaticValueSetterCalled = false;

// 5) Subsequent puts must write the data property, not call the (now stale)
//    custom setter.
for (let i = 0; i < testLoopCount; ++i)
    put(o, i);
assert(o.testStaticValueSetterCalled === false, "stale custom setter was invoked after the property was shadowed");
assert(o.testStaticValueSetFlag === testLoopCount - 1, "the shadowing data property was not written: " + o.testStaticValueSetFlag);
