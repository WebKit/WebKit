//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Stack discipline on the bail path of the generated Transition handler's field-type check. The check must be
// emitted ABOVE makeDefaultScratchAllocator/preserveReusedRegistersByPushing, because it bails to
// m_failAndRepatch, which never runs restoreReusedRegistersByPopping; below the allocator, every bail returns with
// SP still decremented. It needs no scratch register, only valueRegs and an immediate.

// Asserts values and recursion depth after hammering the bail path, which requires a live claim on
// (newStructure, offset), a transition that grows out-of-line storage, and a stored value that misses the claim.
// Mutation-inert: the mechanism self-heals after one miss, since the bailing store withdraws the claim and no
// check is emitted thereafter, so at most one leaked push happens and the frame absorbs it.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Strength() { this.value = 111; }
function Other() { this.a = 1; this.b = 2; this.c = 3; }

// Fill the inline capacity so that adding `f` must allocate out-of-line storage.
function Base() {
    this.p0 = 0; this.p1 = 1; this.p2 = 2; this.p3 = 3;
    this.p4 = 4; this.p5 = 5; this.p6 = 6; this.p7 = 7;
}

function addF(o, v) { o.f = v; }
noInline(addF);

for (let i = 0; i < 200; ++i) {
    const o = new Base();
    addF(o, new Strength());
    shouldBe(o.f.value, 111);
}

// Warm the site so the IC settles into a generated Transition handler carrying the check.
for (let i = 0; i < testLoopCount; ++i) {
    const o = new Base();
    addF(o, new Strength());
    shouldBe(o.f.value, 111);
}

// Hammer the bail path, interleaved with deep-ish calls so a damaged stack has something to corrupt.
function depth3(o) { return depth2(o); }
function depth2(o) { return depth1(o); }
function depth1(o) { return o.f.b + o.p7; }
noInline(depth3);

for (let i = 0; i < testLoopCount; ++i) {
    const o = new Base();
    addF(o, new Other());
    shouldBe(o.f.b, 2);
    shouldBe(depth3(o), 9);
    shouldBe(o.p0, 0);
    shouldBe(o.p7, 7);
}
gc();

for (let i = 0; i < testLoopCount; ++i) {
    const o = new Base();
    addF(o, (i & 1) ? new Other() : new Strength());
    shouldBe(o.p3, 3);
    shouldBe(o.p7, 7);
}
gc();

// A recursive call last: a leaked stack adjustment tends to surface here rather than at the store.
function sum(n) { return n <= 0 ? 0 : n + sum(n - 1); }
noInline(sum);
shouldBe(sum(200), 20100);
