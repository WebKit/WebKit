// Exercise the DFG/FTL constant-folding rule that tries to refine
// ObjectDefinePropertyFromFields into DefineDataProperty /
// DefineAccessorProperty. The rule fires when:
//   - data path: get/set slots are both absent AND enumerable/
//     configurable/writable are each either absent or a known
//     bool constant.
//   - accessor path: value/writable both absent AND get/set are
//     present with AbstractValue proven to be SpecFunction.
// Tests below cover both converted and non-converted shapes; if any
// observable behavior is wrong the tests throw.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg || "") + " expected " + expected + " got " + actual);
}

function shouldThrow(fn, ctor, msg) {
    let threw = false;
    try { fn(); } catch (e) {
        threw = true;
        if (!(e instanceof ctor))
            throw new Error((msg || "") + " wrong error type: " + e);
    }
    if (!threw)
        throw new Error((msg || "") + " expected throw");
}

function readDesc(o, k) {
    return Object.getOwnPropertyDescriptor(o, k);
}

// ----- data: all attrs are constant true (refinable) -----
function testAllTrueData(v) {
    const o = {};
    Object.defineProperty(o, "p", { value: v, writable: true, enumerable: true, configurable: true });
    return o;
}
noInline(testAllTrueData);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testAllTrueData(i);
    shouldBe(o.p, i, "allTrue value");
    const d = readDesc(o, "p");
    shouldBe(d.writable, true, "allTrue writable");
    shouldBe(d.enumerable, true, "allTrue enumerable");
    shouldBe(d.configurable, true, "allTrue configurable");
}

// ----- data: only value present (refinable, partial attrs) -----
function testValueOnly(v) {
    const o = {};
    Object.defineProperty(o, "p", { value: v });
    return o;
}
noInline(testValueOnly);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testValueOnly(i);
    shouldBe(o.p, i, "valueOnly value");
    const d = readDesc(o, "p");
    shouldBe(d.writable, false, "valueOnly writable default");
    shouldBe(d.enumerable, false, "valueOnly enumerable default");
    shouldBe(d.configurable, false, "valueOnly configurable default");
}

// ----- data: writable:false baked in -----
function testWritableFalse(v) {
    const o = {};
    Object.defineProperty(o, "p", { value: v, writable: false, configurable: true });
    return o;
}
noInline(testWritableFalse);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testWritableFalse(i);
    shouldBe(o.p, i, "wFalse value");
    o.p = i + 1; // sloppy: silent no-op
    shouldBe(o.p, i, "wFalse not writable");
    shouldBe(readDesc(o, "p").writable, false, "wFalse writable");
    shouldBe(readDesc(o, "p").configurable, true, "wFalse configurable");
}

// ----- data: generic (no fields) -----
function testGeneric() {
    const o = {};
    Object.defineProperty(o, "p", {});
    return o;
}
noInline(testGeneric);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testGeneric();
    const d = readDesc(o, "p");
    shouldBe(d.value, undefined, "generic value");
    shouldBe(d.writable, false, "generic writable");
    shouldBe(d.enumerable, false, "generic enumerable");
    shouldBe(d.configurable, false, "generic configurable");
}

// ----- NOT refinable: opaque (non-constant) configurable -----
// The slot is present but its value isn't a compile-time bool.
// The rule must bail and keep ObjectDefinePropertyFromFields; behavior
// must still match the spec (ToBoolean on the runtime value).
function testOpaqueConfigurable(v, c) {
    const o = {};
    Object.defineProperty(o, "p", { value: v, configurable: c });
    return o;
}
noInline(testOpaqueConfigurable);
for (let i = 0; i < testLoopCount; ++i) {
    // Alternate truthy/falsy non-bool values so the slot is variable
    // and no constant can be baked in.
    const c = (i & 1) ? "nonempty" : "";
    const o = testOpaqueConfigurable(i, c);
    shouldBe(o.p, i, "opaque value");
    shouldBe(readDesc(o, "p").configurable, !!c, "opaque configurable");
}

// ----- NOT refinable: opaque enumerable -----
function testOpaqueEnumerable(v, e) {
    const o = {};
    Object.defineProperty(o, "p", { value: v, enumerable: e, writable: true, configurable: true });
    return o;
}
noInline(testOpaqueEnumerable);
for (let i = 0; i < testLoopCount; ++i) {
    const e = (i & 1) ? {} : 0;
    const o = testOpaqueEnumerable(i, e);
    shouldBe(readDesc(o, "p").enumerable, !!e, "opaque enumerable");
}

// ----- NOT refinable: opaque writable -----
function testOpaqueWritable(v, w) {
    const o = {};
    Object.defineProperty(o, "p", { value: v, writable: w });
    return o;
}
noInline(testOpaqueWritable);
for (let i = 0; i < testLoopCount; ++i) {
    const w = (i & 1) ? 1 : null;
    const o = testOpaqueWritable(i, w);
    shouldBe(readDesc(o, "p").writable, !!w, "opaque writable");
}

// ----- accessor: get+set present with literal functions -----
function testAccessorLiterals() {
    let box = 0;
    const o = {};
    Object.defineProperty(o, "p", {
        get: function () { return box; },
        set: function (v) { box = v; },
        configurable: true,
    });
    o.p = 42;
    return o.p;
}
noInline(testAccessorLiterals);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(testAccessorLiterals(), 42, "accessor literal");

// ----- accessor: get+set from arguments (may or may not refine) -----
function testAccessorArgs(o, g, s) {
    Object.defineProperty(o, "p", { get: g, set: s, configurable: true });
}
noInline(testAccessorArgs);
for (let i = 0; i < testLoopCount; ++i) {
    let box = 0;
    const o = {};
    testAccessorArgs(o, function () { return box; }, function (v) { box = v; });
    o.p = i;
    shouldBe(o.p, i, "accessor args");
}

// ----- accessor: only get present (one-sided fold to DefineAccessorProperty) -----
// The fold reuses the getter cell as a dummy for the absent setter slot;
// the runtime's toPropertyDescriptor skips the setter when hasSet is unset,
// so the dummy is never observed. Reading returns the getter's value, and
// writing is a silent no-op in sloppy mode / TypeError in strict mode.
function testGetOnly(o, g) {
    Object.defineProperty(o, "p", { get: g, configurable: true });
}
noInline(testGetOnly);
for (let i = 0; i < testLoopCount; ++i) {
    const o = {};
    testGetOnly(o, function () { return 7; });
    shouldBe(o.p, 7, "getOnly");
    shouldBe(readDesc(o, "p").set, undefined, "getOnly no set");
    shouldBe(typeof readDesc(o, "p").get, "function", "getOnly get is function");
    o.p = 99; // sloppy: silent no-op
    shouldBe(o.p, 7, "getOnly sloppy write is no-op");
}

// Strict-mode write on a getter-only accessor must throw TypeError.
function testGetOnlyStrictWrite(o) {
    "use strict";
    o.p = 0;
}
noInline(testGetOnlyStrictWrite);
for (let i = 0; i < testLoopCount; ++i) {
    const o = {};
    testGetOnly(o, function () { return 7; });
    shouldThrow(() => testGetOnlyStrictWrite(o), TypeError, "getOnly strict write");
}

// ----- accessor: only set present (one-sided fold) -----
// Mirrors testGetOnly on the setter side. Reading the property returns
// undefined (no getter installed). Absent getter slot is filled with the
// setter cell as a dummy; toPropertyDescriptor skips it.
function testSetOnly(o, s) {
    Object.defineProperty(o, "p", { set: s, configurable: true });
}
noInline(testSetOnly);
for (let i = 0; i < testLoopCount; ++i) {
    let box = 0;
    const o = {};
    testSetOnly(o, function (v) { box = v; });
    o.p = i;
    shouldBe(box, i, "setOnly setter invoked");
    shouldBe(o.p, undefined, "setOnly read returns undefined");
    shouldBe(readDesc(o, "p").get, undefined, "setOnly no get");
    shouldBe(typeof readDesc(o, "p").set, "function", "setOnly set is function");
}

// ----- accessor: one-sided get that isn't callable (must NOT fold) -----
// `isProvenCallable` on the present side must hold. `get: 42` is a
// descriptor error at runtime (TypeError), and the fold must not convert
// into DefineAccessorProperty (which would silently install a non-callable
// getter). The fallback ObjectDefinePropertyFromFields path throws.
function testBadGetOnly() {
    const o = {};
    Object.defineProperty(o, "p", { get: 42, configurable: true });
}
for (let i = 0; i < testLoopCount; ++i)
    shouldThrow(testBadGetOnly, TypeError, "badGetOnly");

function testBadSetOnly() {
    const o = {};
    Object.defineProperty(o, "p", { set: "nope", configurable: true });
}
for (let i = 0; i < testLoopCount; ++i)
    shouldThrow(testBadSetOnly, TypeError, "badSetOnly");

// ----- accessor: non-callable getter must throw TypeError -----
// Covers the case where the fold would be unsafe: a descriptor with
// `get:` present but not a function. The rule must not convert into
// DefineAccessorProperty (which wouldn't throw), so a TypeError must
// still be observed.
function testBadGetter() {
    const o = {};
    Object.defineProperty(o, "p", { get: 42 });
}
for (let i = 0; i < testLoopCount; ++i)
    shouldThrow(testBadGetter, TypeError, "badGetter");

// ----- mixed data+accessor must still throw TypeError -----
function testMixed() {
    const o = {};
    Object.defineProperty(o, "p", { value: 1, get: function () {} });
}
for (let i = 0; i < testLoopCount; ++i)
    shouldThrow(testMixed, TypeError, "mixed");

// ----- null-prototype descriptor: refinable (no proto to watch) -----
// `Object.create(null)` descriptors have no prototype chain so the fold
// can run without arming propertyDescriptorFastPathWatchpointSet or
// objectPrototypeChainIsSaneWatchpointSet. The extracted
// GetByOffsets still produce correct values, and any descriptor-name
// additions on Object.prototype do not affect the result.
function testNullProtoData(v) {
    const d = Object.create(null);
    d.value = v;
    d.writable = true;
    d.enumerable = true;
    d.configurable = true;
    const o = {};
    Object.defineProperty(o, "p", d);
    return o;
}
noInline(testNullProtoData);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testNullProtoData(i);
    shouldBe(o.p, i, "nullProto value");
    const rd = readDesc(o, "p");
    shouldBe(rd.writable, true, "nullProto writable");
    shouldBe(rd.enumerable, true, "nullProto enumerable");
    shouldBe(rd.configurable, true, "nullProto configurable");
}

// ----- DefineDataProperty -> PutByIdDirect lowering -----
// When the descriptor is {value, writable:true, enumerable:true,
// configurable:true} and we know the base has a structure that
// doesn't already define this property, defineProperty is semantically
// equivalent to a direct data-property put (attrs=0 aka all-default).
// The lowering targets this exact shape; other shapes must keep the
// DefineDataProperty (or ObjectDefinePropertyFromFields) semantics.
function testDefineAllTruePutsDirect(v) {
    const o = {};
    Object.defineProperty(o, "p", { value: v, writable: true, enumerable: true, configurable: true });
    return o;
}
noInline(testDefineAllTruePutsDirect);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testDefineAllTruePutsDirect(i);
    shouldBe(o.p, i, "putDirect value");
    const d = readDesc(o, "p");
    shouldBe(d.writable, true, "putDirect writable");
    shouldBe(d.enumerable, true, "putDirect enumerable");
    shouldBe(d.configurable, true, "putDirect configurable");
    // Reassignment must still work (the created property is writable).
    o.p = i + 1;
    shouldBe(o.p, i + 1, "putDirect reassign");
    // Deletion must still work (configurable).
    delete o.p;
    shouldBe("p" in o, false, "putDirect deletable");
}

// Defaulted descriptor (only value present) must NOT lower to
// PutByIdDirect: defineProperty with a bare {value} creates a non-
// writable, non-enumerable, non-configurable property, whereas
// PutByIdDirect would create an all-default (writable+enumerable+
// configurable) property. The two are observably different.
function testBareValueDoesNotLower(v) {
    const o = {};
    Object.defineProperty(o, "p", { value: v });
    return o;
}
noInline(testBareValueDoesNotLower);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testBareValueDoesNotLower(i);
    shouldBe(o.p, i, "bareValue value");
    const d = readDesc(o, "p");
    shouldBe(d.writable, false, "bareValue non-writable");
    shouldBe(d.enumerable, false, "bareValue non-enumerable");
    shouldBe(d.configurable, false, "bareValue non-configurable");
}

// When the base already has the property, the lowering must bail:
// PutByIdDirect on a non-writable existing property would fail with
// ReadonlyPropertyChangeError, while defineProperty with the same
// attrs would either succeed or throw the spec-defined TypeError.
// Here the existing property is non-writable+configurable, and
// defineProperty must succeed (attrs change + value change allowed
// because configurable is true).
function testExistingPropertyDoesNotLower() {
    const o = {};
    Object.defineProperty(o, "p", { value: 1, writable: false, configurable: true });
    Object.defineProperty(o, "p", { value: 2, writable: true, enumerable: true, configurable: true });
    return o;
}
noInline(testExistingPropertyDoesNotLower);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testExistingPropertyDoesNotLower();
    shouldBe(o.p, 2, "existing value updated");
    shouldBe(readDesc(o, "p").writable, true, "existing writable updated");
}

// Dynamic property name: not a compile-time identifier, can't lower.
function testDynamicPropertyDoesNotLower(v, name) {
    const o = {};
    Object.defineProperty(o, name, { value: v, writable: true, enumerable: true, configurable: true });
    return o;
}
noInline(testDynamicPropertyDoesNotLower);
for (let i = 0; i < testLoopCount; ++i) {
    const name = (i & 1) ? "a" : "b";
    const o = testDynamicPropertyDoesNotLower(i, name);
    shouldBe(o[name], i, "dynamic name value");
    shouldBe(readDesc(o, name).writable, true, "dynamic name writable");
}

// Indexed property: must not go through PutByIdDirect (would need
// PutByVal semantics). The runtime still handles it correctly.
function testIndexedPropertyDoesNotLower(v) {
    const o = {};
    Object.defineProperty(o, "0", { value: v, writable: true, enumerable: true, configurable: true });
    return o;
}
noInline(testIndexedPropertyDoesNotLower);
for (let i = 0; i < testLoopCount; ++i) {
    const o = testIndexedPropertyDoesNotLower(i);
    shouldBe(o[0], i, "indexed value");
    shouldBe(readDesc(o, "0").writable, true, "indexed writable");
}

// ----- descriptor with accessor on a descriptor-field name -----
// Each of these descriptors stores an accessor at one of the names the
// fold otherwise extracts via GetByOffset ("value", "writable", "get").
// If the fold ran, the extracted edge would be the GetterSetter cell
// itself instead of the getter's return value — that would be wrong.
// The existing structure guard (canPerformFastPropertyEnumerationCommon
// rejects accessors) must prevent the fold here, and semantics must
// still go through [[Get]].
function testAccessorOnValue() {
    let reads = 0;
    const d = {};
    Object.defineProperty(d, "value", {
        get: function () { ++reads; return 99; },
    });
    d.writable = true;
    const o = {};
    Object.defineProperty(o, "p", d);
    shouldBe(o.p, 99, "accessorOnValue read via getter");
    shouldBe(reads, 1, "accessorOnValue getter invoked once");
}
noInline(testAccessorOnValue);
for (let i = 0; i < testLoopCount; ++i)
    testAccessorOnValue();

function testAccessorOnWritable() {
    let reads = 0;
    const d = { value: 7 };
    Object.defineProperty(d, "writable", {
        get: function () { ++reads; return false; }, // ToBoolean: false
    });
    const o = {};
    Object.defineProperty(o, "p", d);
    shouldBe(o.p, 7, "accessorOnWritable value");
    shouldBe(reads, 1, "accessorOnWritable getter invoked once");
    o.p = 99; // sloppy: silent no-op
    shouldBe(o.p, 7, "accessorOnWritable observed as non-writable");
}
noInline(testAccessorOnWritable);
for (let i = 0; i < testLoopCount; ++i)
    testAccessorOnWritable();

function testAccessorOnGet() {
    let reads = 0;
    const fn = function () { return 123; };
    const d = {};
    Object.defineProperty(d, "get", {
        get: function () { ++reads; return fn; },
    });
    const o = {};
    Object.defineProperty(o, "p", d);
    shouldBe(o.p, 123, "accessorOnGet getter installed");
    shouldBe(reads, 1, "accessorOnGet meta-getter invoked once");
}
noInline(testAccessorOnGet);
for (let i = 0; i < testLoopCount; ++i)
    testAccessorOnGet();

// ----- dictionary descriptor must not use the structure snapshot -----
// Dictionaries (cacheable or uncacheable) allow property addition in
// place, without a structure transition. A compiled code path that
// extracts fields via offsets pinned to the structure would miss
// properties added after compilation. The fold must bail on any
// dictionary structure.
function testDictionaryMutation(o, d) {
    Object.defineProperty(o, "p", d);
}
noInline(testDictionaryMutation);
{
    const d = { value: 1, writable: true };
    $vm.toCacheableDictionary(d);
    // Warm up with only value + writable present.
    for (let i = 0; i < testLoopCount; ++i) {
        const o = {};
        testDictionaryMutation(o, d);
        shouldBe(o.p, 1, "dict phase1 value");
        shouldBe(readDesc(o, "p").writable, true, "dict phase1 writable");
        shouldBe(readDesc(o, "p").enumerable, false, "dict phase1 enumerable default");
        shouldBe(readDesc(o, "p").configurable, false, "dict phase1 configurable default");
    }
    // Add a new field in place — dictionary structure is mutated,
    // no transition. A stale-snapshot fold would still treat enumerable
    // as absent here.
    d.enumerable = true;
    d.configurable = true;
    for (let i = 0; i < testLoopCount; ++i) {
        const o = {};
        testDictionaryMutation(o, d);
        shouldBe(readDesc(o, "p").enumerable, true, "dict phase2 enumerable");
        shouldBe(readDesc(o, "p").configurable, true, "dict phase2 configurable");
    }
}

// ----- symbol key (covers non-string property path) -----
const SYM = Symbol("p");
function testSymbolKey(v) {
    const o = {};
    Object.defineProperty(o, SYM, { value: v, writable: true, enumerable: true, configurable: true });
    return o[SYM];
}
noInline(testSymbolKey);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(testSymbolKey(i), i, "symbol key");

// ----- cross-realm descriptor must not fold -----
// The fold compares descriptorStructure->storedPrototype() against the
// compile-time globalObject->objectPrototype(). A descriptor created in
// another realm inherits from that realm's Object.prototype, so the
// identity check fails and the fold bails to the generic runtime path.
// Semantics must still be correct.
if (typeof createGlobalObject === "function") {
    const other = createGlobalObject();
    function testCrossRealm(v) {
        const d = new other.Object();
        d.value = v;
        d.writable = true;
        d.enumerable = true;
        d.configurable = true;
        const o = {};
        Object.defineProperty(o, "p", d);
        return o;
    }
    noInline(testCrossRealm);
    for (let i = 0; i < testLoopCount; ++i) {
        const o = testCrossRealm(i);
        shouldBe(o.p, i, "crossRealm value");
        const descOut = readDesc(o, "p");
        shouldBe(descOut.writable, true, "crossRealm writable");
        shouldBe(descOut.enumerable, true, "crossRealm enumerable");
        shouldBe(descOut.configurable, true, "crossRealm configurable");
    }
}
