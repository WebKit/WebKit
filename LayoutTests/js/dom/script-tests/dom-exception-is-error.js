description(
"Tests that Error.isError() returns true for DOMException and its subclasses, including on the DFG/FTL JIT path. The intrinsic that inlines Error.isError() cannot be reached from JSC-shell tests since those cannot construct a DOMException, so this test warms up the JIT from a DOM context."
);

function test(v) {
    return Error.isError(v);
}
if (self.testRunner)
    testRunner.neverInlineFunction(test);

var cases = [
    [new DOMException(), true],
    [new TypeError(), true],
    [new Error(), true],
    [{}, false],
    [5, false],
    [null, false],
    [undefined, false],
    ["string", false],
];

// WebTransportError is a DOMException subclass, but is only present when WebTransport is enabled.
// Include it when available so the JIT path exercises a subclass too.
if (window.WebTransportError)
    cases.push([new WebTransportError(), true]);

// Run hot enough to tier up through the DFG to the FTL, so the inlined
// ErrorIsErrorIntrinsic is exercised rather than just the LLInt/baseline path.
var iterations = 200000;
var allStable = true;
for (var i = 0; i < iterations; ++i) {
    var c = cases[i % cases.length];
    if (test(c[0]) !== c[1])
        allStable = false;
}

shouldBeTrue("allStable");

// Confirm we actually reached the JIT (so this test keeps its value); only
// meaningful under the test harness.
if (self.testRunner)
    shouldBeTrue("testRunner.numberOfDFGCompiles(test) > 0");

// Spell out the interesting cases so the coverage is explicit: DOMException, a DOMException
// subclass (guarded so the output is identical whether or not WebTransport is enabled), a
// regular Error, and non-errors.
shouldBeTrue("Error.isError(new DOMException())");
shouldBeTrue("!window.WebTransportError || Error.isError(new WebTransportError())");
shouldBeTrue("Error.isError(new TypeError())");
shouldBeFalse("Error.isError(new (class extends Object {})())");
shouldBeFalse("Error.isError({})");
