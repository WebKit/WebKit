// Regression test for the DFG ObjectDefineProperty descriptor-field fold in
// DFGConstantFoldingPhase. The fold reads the descriptor's structure via
// AbstractValue::m_structure and then emits KnownCellUse GetByOffset loads,
// converting the node into ObjectDefinePropertyFromFields.
//
// AbstractValue::m_structure only describes the *cell* portion of a value.
// When a value merges an object literal (a concrete FinalObject structure)
// with `undefined` on another path, the merged abstract value keeps the
// object structure while its type becomes SpecFinalObject | SpecOther — i.e.
// it is NOT proven to be an object. Emitting KnownCellUse loads (which perform
// no speculation) on that value would dereference the `undefined` immediate as
// a cell and crash.
//
// The fold must re-emit the ObjectUse speculation the original
// ObjectDefineProperty node carried, so a non-object descriptor OSR-exits to
// the runtime (which throws the spec TypeError) instead of crashing. This test
// exercises exactly that merge: the descriptor is either a full data descriptor
// or `undefined`. It must not crash; the undefined path must throw TypeError,
// and the object path must define the property normally.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg || "") + " expected " + expected + " got " + actual);
}

function shouldThrow(fn, ctor, msg) {
    let threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (!(e instanceof ctor))
            throw new Error((msg || "") + " wrong error type: " + e);
    }
    if (!threw)
        throw new Error((msg || "") + " expected throw");
}

// `descriptor` is a Phi merging a FinalObject literal and `undefined`.
function defineWithMaybeDescriptor(target, useObject, v) {
    const descriptor = useObject
        ? { value: v, writable: true, enumerable: true, configurable: true }
        : undefined;
    Object.defineProperty(target, "p", descriptor);
}
noInline(defineWithMaybeDescriptor);

for (let i = 0; i < testLoopCount; ++i) {
    if ((i & 3) === 0) {
        const o = {};
        shouldThrow(() => defineWithMaybeDescriptor(o, false, i), TypeError, "undefined descriptor");
        shouldBe("p" in o, false, "undefined descriptor did not define");
    } else {
        const o = {};
        defineWithMaybeDescriptor(o, true, i);
        shouldBe(o.p, i, "object descriptor value");
        const d = Object.getOwnPropertyDescriptor(o, "p");
        shouldBe(d.writable, true, "object descriptor writable");
        shouldBe(d.enumerable, true, "object descriptor enumerable");
        shouldBe(d.configurable, true, "object descriptor configurable");
    }
}

// Reproduction shaped like the original report: the descriptor merge happens
// deep inside a JSON.parse reviver so the crashing frame is reached through the
// LLInt->baseline->DFG tier-up path.
let tick = 0;

function pressure(n) {
    if ((n & 7) !== 0)
        return;

    let descriptor = (n % 3) === 0
        ? { value: n, writable: true, enumerable: true, configurable: true }
        : undefined;

    Object.defineProperty(this, "pressure", descriptor);
}

function reviver(key, value) {
    tick++;
    pressure(tick);
    return value;
}

for (let i = 0; i < testLoopCount; ++i) {
    try {
        JSON.parse("[0]", reviver);
    } catch {
    }
}
