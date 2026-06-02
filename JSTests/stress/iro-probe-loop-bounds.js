//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// Probing arr.length at the loop top and the induction variable inside lets us
// assert `i < len`: CSE makes the in-loop arr.length the same node the probe
// wraps, so the relation is named on both sides.
//
// IRO widens loop bounds, so the tight offset form (`i < len - 1`, e.g. from
// `i + 1 < arr.length`) is not derived; it keeps only `i < len` / `i <= len`
// for the induction variable. That is a limit of the analysis, not the dump,
// so the test asserts only the bounds IRO actually produces.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(arr) {
    const len = $vm.probe("len", arr.length);
    let sum = 0;
    for (let i = 0; i < arr.length; i++) {
        const pi = $vm.probe("i", i);
        sum += arr[i] | 0;
    }
    return sum;
}
noInline(fn);

const a = [1, 2, 3, 4, 5, 6, 7, 8];
for (let k = 0; k < testLoopCount; k++) fn(a);
const iro = makeIROHelper(fn);

// The induction variable is bounded by the probed length and non-negative.
// These hold at the "i" point (inside the loop); the "len" probe sits at the
// loop top, before i exists, so the relation is anchored at "i".
iro.assertRel({ at: "i", lhs: "i", rel: "<", rhs: "len" });
iro.assertRel({ at: "i", lhs: "i", rel: ">=", rhs: { const: 0 } });
iro.assertRel({ at: "i", lhs: "len", rel: ">", rhs: "i" });
