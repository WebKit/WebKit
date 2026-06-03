//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// IRO's ArithBitOr rule: `a | C` with C provably negative is negative and
// >= C, so `x | -16` is in [-16, -1]. Pins the proven range so the rule
// silently not firing fails here (the correctness-only b4cca18 test wouldn't).

load("./resources/iro-test-helpers.js", "caller relative");

function bitOrNeg(x) {
    return $vm.probe("y", (x | 0) | -16);
}
noInline(bitOrNeg);

for (let i = 0; i < 300000; ++i)
    bitOrNeg(i);

const iro = makeIROHelper(bitOrNeg);
const r = iro.range("y");
if (!r || r.max !== -1)
    throw new Error('expected IRO to prove `x | -16` is negative (max -1), got '
        + JSON.stringify(r) + ' — the ArithBitOr sign-bit rule did not fire');
if (r.min !== -16)
    throw new Error('expected IRO to prove `x | -16` >= -16 (min -16), got '
        + JSON.stringify(r) + ' — the ArithBitOr lower-bound rule did not fire');

iro.assertRel({ at: "y", lhs: "y", rel: "<", rhs: { const: 0 } });
iro.assertRel({ at: "y", lhs: "y", rel: ">=", rhs: { const: -16 } });

print("PASS");
