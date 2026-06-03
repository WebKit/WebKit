//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// Probes the loop induction variable to check IRO derives the facts that let
// it elide a CheckInBounds: i >= 0 and i < arr.length. (We probe `i` alongside
// the indexed read rather than as the index, which would confuse tier-up.)

load("./resources/iro-test-helpers.js", "caller relative");

function warmup(fn, args) {
    for (let i = 0; i < 1000000; ++i) fn(...args);
}

function fnElided(arr) {
    let sum = 0;
    for (let i = 0; i < arr.length; ++i) {
        sum += arr[i] + $vm.probe("p", i);
    }
    return sum;
}
noInline(fnElided);

const arr = new Int32Array(8);
for (let i = 0; i < arr.length; ++i) arr[i] = i + 1;
warmup(fnElided, [arr]);
{
    const iro = makeIROHelper(fnElided);
    const r = iro.range("p");
    if (!r || r.min < 0)
        throw new Error("fnElided: expected probe range min >= 0, got "
            + JSON.stringify(r));
    iro.assertRel({ at: "p", lhs: "p", rel: ">=", rhs: { const: 0 } });
    let want = 0;
    for (let i = 0; i < arr.length; ++i) want += arr[i] + i;
    const got = fnElided(arr);
    if (got !== want)
        throw new Error("fnElided runtime mismatch: got " + got + " want " + want);
}

// Negative: a free parameter `i`. IRO has no facts on it.
function fnNotElided(arr, i) {
    const tmp = $vm.probe("p", i);
    return arr[i] + tmp;
}
noInline(fnNotElided);
warmup(fnNotElided, [arr, 0]);
{
    const iro = makeIROHelper(fnNotElided);
    const r = iro.range("p");
    if (r && r.min >= 0)
        throw new Error("fnNotElided: expected IRO to have no min>=0 fact on "
            + "free param i, got " + JSON.stringify(r));
}

print("PASS");
