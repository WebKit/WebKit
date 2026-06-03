//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// Soundness check for IRO's shadow-projection read guards. `sim` is an
// un-optimized twin of `fn` that records each probe's true runtime range; the
// test fails if IRO's proven range for any probe doesn't contain it.

load("./resources/iro-test-helpers.js", "caller relative");

// ---- compiled, probed version --------------------------------------------
function fn(n) {
    let a = 100;                 // c1 (entry constant)
    let b = 100;
    let s = 0;
    let g = 0;
    while (g < n) {
        $vm.probe("b_in", b);    // b as a Phi
        b = a;                   // b := a
        s = a + 50;              // ArithAdd(a, 50) -> expandedRangeFor(a)
        $vm.probe("a_in", a);    // a as a Phi
        $vm.probe("s_in", s);
        a = 200;                 // c2 (reset a to a different constant)
        g = g + 1;
    }
    $vm.probe("b_out", b);
    $vm.probe("a_out", a);
    return b + a + s;
}
noInline(fn);

// ---- tracking twin (never optimized) -> true ranges ----------------------
const actual = new Map();
function track(id, v) {
    let r = actual.get(id);
    if (!r) actual.set(id, { min: v, max: v });
    else { if (v < r.min) r.min = v; if (v > r.max) r.max = v; }
}
function sim(n) {
    let a = 100, b = 100, s = 0, g = 0;
    while (g < n) {
        track("b_in", b);
        b = a;
        s = a + 50;
        track("a_in", a);
        track("s_in", s);
        a = 200;
        g = g + 1;
    }
    track("b_out", b);
    track("a_out", a);
    return b + a + s;
}
noInline(sim);
noFTL(sim);
noDFG(sim);

// Drive both with the same inputs.
const Ns = [1, 2, 3, 8];
for (let it = 0; it < 200000; ++it) fn(Ns[it & 3]);
for (const n of Ns) sim(n);

const iro = makeIROHelper(fn);

for (const id of iro.probes.keys()) {
    const ir = iro.range(id);
    const ac = actual.get(id);
    if (!ir || !ac)
        continue;
    if (ac.min < ir.min || ac.max > ir.max)
        throw new Error("UNSOUND range for probe " + JSON.stringify(id)
            + ": IRO proved " + ir.min + ".." + ir.max
            + " but the value actually reaches " + ac.min + ".." + ac.max
            + ". A shadow-projection read guard has regressed.\n"
            + String(iro.relsAt(id)));
}

print("PASS");
