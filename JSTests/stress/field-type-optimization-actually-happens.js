//@ runDefault("--useFieldTypeAssumptions=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// runDefault, not requireOptions: this test asserts that a compilation adopted a claim, so it legitimately fails
// under lockdown, no-llint and ftl-eager -- no JIT, or compilation before any claim has formed.
// --useConcurrentJIT=0 makes tier-up synchronous so the counter has moved by the time the assertion runs.

// The only test that asserts the optimization does anything: every other field-type test passes with
// --useFieldTypeAssumptions=0, so deleting the mechanism entirely would leave the suite green.
// $vm.fieldTypeNarrowCount() counts compiler adoptions of a claim (Graph::fieldTypeAssumptionValue), so it stops
// moving if a claim never forms, a consumer always declines, or a store path always surrenders. Deliberately no
// --validateFieldTypes: the point here is the optimization, not soundness.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Pointee() { this.a = 42; }
function Holder(p) { this.f = p; }

const holders = [];
for (let i = 0; i < 256; ++i)
    holders.push(new Holder(new Pointee()));

const before = $vm.fieldTypeNarrowCount();

// With a claim on (Holder-shape, f) -> Pointee the compiler can drop the CheckStructure on the loaded value.
function read(h) { return h.f.a; }
noInline(read);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(read(holders[i & 255]), 42);

// A second, independent site, so the assertion does not depend on one compilation surviving.
function readAgain(h) { return h.f.a + 1; }
noInline(readAgain);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readAgain(holders[i & 255]), 43);

const after = $vm.fieldTypeNarrowCount();

if (!(after > before)) {
    throw new Error("field types never narrowed anything: fieldTypeNarrowCount stayed at " + before
        + ". The mechanism is not doing its job -- a claim is not forming, the consumer is declining, or a store "
        + "path is surrendering the claim. Every other field-type test would still pass in this state, which is "
        + "exactly why this test exists.");
}

// Values must still be right, so a counter moving for the wrong reason is not mistaken for success.
for (let i = 0; i < 256; ++i) {
    shouldBe(holders[i].f.a, 42);
    shouldBe(read(holders[i]), 42);
}
