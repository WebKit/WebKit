//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// `x` is always negative here, so IRO must prove a negative lower bound
// (x >= -10) and never flip it to x >= 0. Asserts each probe's min is in
// [-10, 0): catches an unsound x >= 0 fact, and a lost bound (min == INT_MIN).

load("./resources/iro-test-helpers.js", "caller relative");

function fn() {
    let i = 0;
    let x = -10;
    $vm.probe("x0", x);
    while (i < 2) {
        let bool = opaque();
        $vm.probe("x1", x);

        if (bool) {
            $vm.probe("x2", x);
            x = -1
        } else {
            $vm.probe("x3", x);
            x = x + 1
        }
        $vm.probe("x4", x);
        i = i + 1;
    }
}
noInline(fn);

let b = true
function opaque() { return b }
noInline(opaque);

for (let it = 0; it < 200000; ++it) {
    b = false;
    fn();
    b = true
    fn()
}

const iro = makeIROHelper(fn);

const r0 = iro.range("x0");
if (!r0 || r0.min !== -10 || r0.max !== -10)
    throw new Error('x0: expected range [-10,-10], got ' + JSON.stringify(r0));

for (const id of ["x1", "x2", "x3", "x4"]) {
    const r = iro.range(id);
    if (!r)
        throw new Error(id + ": expected IRO to record a range");
    if (r.min >= 0)
        throw new Error(id + ": IRO unsoundly proved a non-negative lower bound "
            + "(min=" + r.min + "); x is signed and is negative here");
    if (r.min < -10)
        throw new Error(id + ": expected IRO to prove a lower bound of at least "
            + "-10 (got min=" + r.min + ") — the negative-bound fact was lost");
}

print("PASS");
