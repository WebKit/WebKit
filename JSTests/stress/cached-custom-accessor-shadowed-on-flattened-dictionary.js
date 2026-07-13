//@ runDefault("--useConcurrentJIT=false")

// Regression test for a stale-JIT-cache bug: a custom accessor served from a
// static property table is/ cached by the JIT as a constant getter guarded only
// by a structure check. On a dictionary structure a property can be added
// *in place* (no structure transition) to shadow the custom accessor, which
// would leave the cached getter call stale. Fresh dictionaries are flattened
// before caching so that adds transition the structure, but a structure that
// has already been flattened once is not re-flattened and can become a dictionary
// again (which is what long-lived global objects do), reopening the hole.

function assert(b, msg) {
    if (!b)
        throw new Error("FAILED: " + (msg || ""));
}

// testStaticValueNoSetter is a non-reified static custom (getter returns
// undefined, no setter). It is cached as a CustomValueGetter, exactly like a
// DOM constructor. Writing to it shadows it with a real own data property.
let o = $vm.createStaticCustomValue();

function get(obj) { return obj.testStaticValueNoSetter; }
noInline(get);

// 1) Make it a cacheable dictionary, then warm up so the get-by-id inline cache
//    repatches and *flattens* the dictionary (sets hasBeenFlattenedBefore).
o = $vm.toCacheableDictionary(o);
for (let i = 0; i < testLoopCount; ++i)
    assert(get(o) === undefined, "warmup1");

// 2) Convert it back to a cacheable dictionary. The new dictionary structure is
//    created via Structure::create(vm, previous), which inherits
//    hasBeenFlattenedBefore -- so we now have a cacheable dictionary that has
//    already been flattened, just like a churned Window global object.
o = $vm.toCacheableDictionary(o);

// 3) Warm up again so the custom getter is (re)cached against this
//    flattened-before dictionary. prepareChainForCaching will not re-flatten it.
for (let i = 0; i < testLoopCount; ++i)
    assert(get(o) === undefined, "warmup2");

// 4) Shadow the custom accessor with a real own data property. On a dictionary
//    this is an in-place add with NO structure transition (same StructureID).
o.testStaticValueNoSetter = 42;

// 5) The JIT must observe the shadowing property, not keep calling the cached
//    (now stale) custom getter.
for (let i = 0; i < testLoopCount; ++i)
    assert(get(o) === 42, "stale custom getter at i=" + i + ": got " + get(o));
