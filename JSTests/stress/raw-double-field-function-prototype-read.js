//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=0")
//
// JSFunction::getOwnPropertySlot's non-reified-prototype arm must read with the raw-double-aware getDirect.
//
// It did not. JSFunction.cpp, inside `if (propertyName == vm.propertyNames->prototype)` ->
// `if (thisObject->mayHaveNonReifiedPrototype())`:
//
//     PropertyOffset offset = thisObject->getDirectOffset(vm, propertyName, attributes);
//     ...
//     slot.setValue(thisObject, attributes, thisObject->getDirect(offset), offset);   // <-- UNCHECKED
//
// `f.prototype = 1.5` is an ordinary own-property store, so the slot is marked Double-represented and holds
// bits(1.5). Reading it unchecked yields bits(1.5) interpreted as a JSValue, which is bits(1.5) - 2^49 as a
// double: 1.375. Silent, no crash -- the same defect class as the displayName read next door, but on the
// wrong-value side rather than the fake-cell side.
//
// This is the same bug class as repro/bugs/closed/03 (private fields) and 04 (Proxy handler traps): a C++
// reader that has the Structure in hand and calls the unchecked overload anyway.

function shouldBe(actual, expected, what) {
    if (!Object.is(actual, expected))
        throw new Error((what ? what + ": " : "") + "got " + actual + " expected " + expected);
}

const VALUES = [
    1.5, -1.5, 0.1, 1 / 3, Math.PI,
    2000, 4294967296, 1e21,
    5e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
    -0.0, 0.0, Infinity, -Infinity,
];

for (const v of VALUES) {
    function f() { }
    f.prototype = v;
    shouldBe(f.prototype, v, "f.prototype = " + v);
    // Reflect and the descriptor path reach the same slot through different readers.
    shouldBe(Reflect.get(f, "prototype"), v, "Reflect.get for " + v);
    shouldBe(Object.getOwnPropertyDescriptor(f, "prototype").value, v, "descriptor for " + v);
}

// NaN separately: only that it stays a NaN.
{
    function f() { }
    f.prototype = NaN;
    if (f.prototype === f.prototype)
        throw new Error("f.prototype = NaN read back as " + f.prototype);
}

// Non-numbers must be unaffected -- a real prototype object is the normal case.
{
    function f() { }
    const proto = { tag: "proto" };
    f.prototype = proto;
    shouldBe(f.prototype, proto, "object prototype");
    shouldBe(new f() instanceof f, true, "instanceof still works");
}

// Hot, so the read also happens from compiled code and through the inline caches.
{
    function f() { }
    f.prototype = 2.25;
    let bad = 0;
    for (let i = 0; i < 100000; ++i) {
        if (f.prototype !== 2.25) ++bad;
    }
    shouldBe(bad, 0, "hot f.prototype reads");
}

// A class constructor reifies its prototype from bytecode, so it takes the other arm -- included as the
// control that the fix did not change that path.
{
    class C { }
    if (typeof C.prototype !== "object")
        throw new Error("class prototype is " + typeof C.prototype);
}

print("PASS");
