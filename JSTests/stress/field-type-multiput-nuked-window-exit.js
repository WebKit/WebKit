//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// FTL MultiPutByOffset against a live claim: needs a polymorphic store site, a Transition variant that grows
// out-of-line storage, and a stored value that misses the claim so the speculation fires. Asserts that stores and
// reads stay correct across three shapes and that nothing crashes.

// The field-type check is emitted at the top of the per-variant block, before storageForTransition: that call
// nukes the structureID and swaps the butterfly, and nothing may exit before the closing structure store.
// mayExit() cannot catch such an exit because it is a per-node query. This test is mutation-inert -- harm needs a
// collection inside a few-instruction window, which a test cannot steer a GC into.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Strength() { this.value = 111; }
function Other() { this.a = 1; this.b = 2; this.c = 3; }

// Both shapes are at inline capacity, so adding `f` reallocates out-of-line storage.
function ShapeA() {
    this.p0 = 0; this.p1 = 1; this.p2 = 2; this.p3 = 3;
    this.p4 = 4; this.p5 = 5; this.p6 = 6; this.p7 = 7;
}
function ShapeB() {
    this.q0 = 0; this.q1 = 1; this.q2 = 2; this.q3 = 3;
    this.q4 = 4; this.q5 = 5; this.q6 = 6; this.q7 = 7;
}

// One store site, two base structures, so this lowers to MultiPutByOffset with two Transition variants.
function put(o, v) { o.f = v; }
noInline(put);

for (let i = 0; i < 200; ++i) {
    const a = new ShapeA(); put(a, new Strength()); shouldBe(a.f.value, 111);
    const b = new ShapeB(); put(b, new Strength()); shouldBe(b.f.value, 111);
}

// Warm hard so the site reaches FTL.
for (let i = 0; i < testLoopCount; ++i) {
    const o = (i & 1) ? new ShapeA() : new ShapeB();
    put(o, new Strength());
    shouldBe(o.f.value, 111);
}
gc();

for (let i = 0; i < testLoopCount; ++i) {
    const o = (i & 1) ? new ShapeA() : new ShapeB();
    put(o, new Other());
    shouldBe(o.f.b, 2);
    shouldBe(o.f.value, undefined);
}
gc();

// Again with a gc() inside the loop, to maximise the chance of a collection landing near the store.
for (let i = 0; i < 400; ++i) {
    const o = (i & 1) ? new ShapeA() : new ShapeB();
    put(o, new Other());
    if (!(i & 63))
        gc();
    shouldBe(o.f.b, 2);
}
gc();

// A third shape introduced late, so the site is re-profiled while claims are live.
function ShapeC() {
    this.r0 = 0; this.r1 = 1; this.r2 = 2; this.r3 = 3;
    this.r4 = 4; this.r5 = 5; this.r6 = 6; this.r7 = 7;
}
for (let i = 0; i < testLoopCount; ++i) {
    const o = (i % 3 === 0) ? new ShapeA() : (i % 3 === 1) ? new ShapeB() : new ShapeC();
    put(o, (i & 1) ? new Other() : new Strength());
    shouldBe(o.p0 === 0 || o.q0 === 0 || o.r0 === 0, true);
}
gc();
