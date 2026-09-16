//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=1")
//@ runDefault("--useDoubleFieldRepresentation=1", "--useRawDoubleFieldStorage=0")
//
// JSFunction::getCalculatedDisplayName must read `displayName` with the raw-double-aware getDirect.
//
// It did not. JSFunction.cpp had, with `structure` already the local two lines above:
//
//     PropertyOffset offset = structure->getConcurrently(vm.propertyNames->displayName.impl(), attributes);
//     if (offset != invalidOffset && !(attributes & (Accessor | CustomAccessorOrValue))) {
//         JSValue displayName = object->getDirect(offset);          // <-- UNCHECKED
//         if (displayName && displayName.isString())
//             return asString(displayName)->tryGetValueWithoutGC();
//
// `displayName` is an ordinary own property, so `f.displayName = <double>` marks it Double-represented and the
// slot holds bare IEEE-754 bits. The unchecked read hands those bits back as a JSValue. isCell() is
// !(bits & (NumberTag | OtherTag)), so ANY bit pattern below 2^49 with bit 1 clear -- every small positive
// subnormal -- is classified as a cell, and isString() then dereferences a pointer the script chose outright
// through a Float64Array. 5e-324 has bits 0x1, so the faulting address is 0x1.
//
// Reached from StackVisitor/StackFrame whenever a stack trace has to name the function, i.e. from ordinary
// `new Error().stack`. The function must be ON the stack: throwing from inside it is what makes the trace
// name it. An earlier attempt to reproduce this threw from a different frame and saw nothing.
//
// This path also runs when the mutator is not running (lazy stack-trace generation), which is why the fix uses
// getDirect(Structure&, PropertyOffset) rather than re-deriving anything: Structure::isRawDoubleOffset is
// lock-free and allocation-free by construction.

function shouldBe(actual, expected, what) {
    if (!Object.is(actual, expected))
        throw new Error((what ? what + ": " : "") + "got " + actual + " expected " + expected);
}

// The cell-shaped payloads: bits below 2^49 with bit 1 clear. Each would be dereferenced as a JSCell*.
const CELL_SHAPED = [
    5e-324,                    // bits 0x0000000000000001
    2.5e-323,                  // bits 0x0000000000000005
    1.390671161567e-309,       // bits 0x0001000000000000
    0.0,                       // bits 0x0000000000000000 -- the EMPTY JSValue
];

for (const payload of CELL_SHAPED) {
    const f = function () { throw new Error("boom"); };
    f.displayName = payload;
    let stack = null;
    try { f(); } catch (e) { stack = String(e.stack); }
    if (typeof stack !== "string" || !stack.length)
        throw new Error("no stack for displayName = " + payload);
    // The value must still read back exactly; the stack must name the function rather than crash.
    shouldBe(f.displayName, payload, "displayName round trip for " + payload);
}

// Ordinary values must be unaffected, and a real string displayName must still be honoured.
{
    const f = function () { throw new Error("boom"); };
    f.displayName = 1.5;
    try { f(); } catch (e) { String(e.stack); }
    shouldBe(f.displayName, 1.5, "non-cell-shaped double");
}
{
    const f = function () { throw new Error("boom"); };
    f.displayName = "myDisplayName";
    let stack = "";
    try { f(); } catch (e) { stack = String(e.stack); }
    shouldBe(f.displayName, "myDisplayName", "string displayName preserved");
}

// Hot, so the JIT tiers up over a shape carrying the marked field and the read happens from compiled frames.
{
    const f = function () { throw new Error("boom"); };
    f.displayName = 5e-324;
    let n = 0;
    for (let i = 0; i < 2000; ++i) {
        try { f(); } catch (e) { if (String(e.stack).length) ++n; }
    }
    shouldBe(n, 2000, "hot stack-trace loop");
    shouldBe(f.displayName, 5e-324, "displayName after the hot loop");
}

print("PASS");
