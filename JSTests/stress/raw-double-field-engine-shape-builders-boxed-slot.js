//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=0")
//
// EVERY lazily-built engine-internal shape must be built with Structure::addPropertyTransitionForBoxedSlot.
//
// StructureTransitionTable::Hash::Key masks PropertyAttribute::RepresentationDouble out of PropertyAddition
// keys -- deliberately and permanently, because putting it in the key forks one source shape into two and
// measured Box2D -11.3%. The consequence is that `addPropertyTransition(base, name, 0)` can be served a
// RAW-CLAIMING sibling that an unrelated script store of a double created earlier under the same name on the
// same base. The engine builder then stores a non-number into that slot by offset and hits the fail-closed
// RELEASE_ASSERT in JSObject::putDirectOffsetRawDoubleAware -- which is NOT compiled out in release, so this
// is a process abort reachable from two lines of ordinary JavaScript.
//
// addPropertyTransitionForBoxedSlot exists for exactly this and asks for the boxed sibling instead.
//
// HISTORY, and why this test covers a whole family rather than one site. The first fix converted only the two
// ObjectConstructor.h descriptor builders (repro/bugs/closed/07). Six other files had the identical idiom and
// were left: JSPromise.cpp, JSPromiseConstructor.cpp, IteratorOperations.cpp, RegExpMatchesArray.cpp,
// RegExp.cpp, IntlPartObject.cpp and IntlSegmentDataObject.cpp -- 37 call sites in total.
//
// REACHABILITY IS AN ACCIDENT, WHICH IS WHY ALL OF THEM WERE CONVERTED AND WHY THIS TEST DOES NOT ONLY COVER
// THE ONES THAT FIRE TODAY. A builder is only script-reachable if a script can obtain the same base
// Structure, which means matching BOTH the prototype and the inline capacity:
//   - JSPromise.cpp uses JSFinalObject::defaultInlineCapacity + objectPrototype -> `Object.create(Object.prototype)`
//     reaches it, and it aborted;
//   - RegExp.cpp's named-capture-groups object has a NULL prototype -> `Object.create(null)` reaches it, and it
//     aborted. (An earlier triage used Object.prototype here, saw nothing, and wrongly cleared the site.)
//   - IntlPartObject/IntlSegmentDataObject/IteratorOperations/JSPromiseConstructor use inline capacities 2, 3
//     and 4, which a plain script object does not get today. They are covered below anyway: the capacity that
//     makes them unreachable is not a guarantee, it is an implementation detail of the allocation profile.

function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error((what ? what + ": " : "") + "got " + actual + " expected " + expected);
}

// Poison every property name an engine builder uses, on BOTH reachable bases, with a double.
const NAMES = [
    "value", "writable", "enumerable", "configurable", "get", "set",     // property descriptors
    "resolve", "reject", "promise",                                      // promise capability
    "status", "reason",                                                  // Promise.allSettled results
    "done",                                                              // iterator results
    "index", "input", "groups", "indices",                               // RegExp match results
    "type", "source", "unit",                                            // Intl parts
    "segment", "isWordLike",                                             // Intl segment data
    "foo", "bar",                                                        // script-chosen capture-group names
];

for (const name of NAMES) {
    const withProto = Object.create(Object.prototype);
    withProto[name] = 1.5;
    const nullProto = Object.create(null);
    nullProto[name] = 1.5;
}

// ---- descriptor objects (the originally fixed sites)
shouldBe(Object.getOwnPropertyDescriptor({ x: "a string" }, "x").value, "a string", "data descriptor");
{
    const src = { get x() { return 1; }, set x(v) { } };
    shouldBe(typeof Object.getOwnPropertyDescriptor(src, "x").get, "function", "accessor descriptor");
}

// ---- promise capability objects
shouldBe(Object.keys(Promise.withResolvers()).sort().join(","), "promise,reject,resolve", "withResolvers");
{
    class MyPromise extends Promise { }
    MyPromise.all([Promise.resolve(1)]);          // species constructor -> createPromiseCapability
    MyPromise.race([Promise.resolve(1)]);
}
Promise.resolve(1).then(function () { });

// ---- Promise.allSettled result objects
Promise.allSettled([1, Promise.reject(new Error("x"))]);

// ---- iterator results
{
    function* gen() { yield 1; yield 2; }
    const it = gen();
    const r = it.next();
    shouldBe(r.value, 1, "iterator result value");
    shouldBe(r.done, false, "iterator result done");
    shouldBe(it.next().value, 2, "second iterator result");
    shouldBe(it.next().done, true, "iterator exhausted");
    let sum = 0;
    for (const v of gen()) sum += v;
    shouldBe(sum, 3, "for-of over the generator");
}

// ---- RegExp match results, including named groups and indices
{
    const m = /(?<foo>a)(?<bar>b)/.exec("ab");
    shouldBe(m.groups.foo, "a", "named group foo");
    shouldBe(m.groups.bar, "b", "named group bar");
    shouldBe(m.index, 0, "match index");
    shouldBe(m.input, "ab", "match input");
    const d = /(?<foo>a)/d.exec("a");
    shouldBe(d.indices[0][0], 0, "match indices");
    shouldBe([..."aa".matchAll(/(?<foo>a)/g)].length, 2, "matchAll");
    shouldBe("ab".replace(/(?<foo>a)/, "$<foo>!"), "a!b", "replace with a named group");
    shouldBe("a,b".split(/(?<foo>,)/).length, 3, "split with a named group");
}

// ---- Intl part objects and segment data
{
    const parts = new Intl.NumberFormat("en").formatToParts(1234.5);
    shouldBe(parts[0].type, "integer", "NumberFormat part type");
    if (typeof Intl.Segmenter === "function") {
        const seg = [...new Intl.Segmenter("en", { granularity: "word" }).segment("hi there")];
        shouldBe(typeof seg[0].segment, "string", "Segmenter segment");
        shouldBe(typeof seg[0].index, "number", "Segmenter index");
    }
    if (typeof Intl.RelativeTimeFormat === "function")
        shouldBe(typeof new Intl.RelativeTimeFormat("en").formatToParts(1, "day")[0].type, "string", "RelativeTimeFormat part");
}

// ---- run each family again, hot, so the structures are also exercised from compiled code
for (let i = 0; i < 2000; ++i) {
    Object.getOwnPropertyDescriptor({ x: i }, "x");
    Promise.withResolvers();
    /(?<foo>a)/.exec("a");
}

print("PASS");
