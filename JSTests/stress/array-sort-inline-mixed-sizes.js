//@ runDefault
//@ requireOptions("--useConcurrentJIT=0")
//
// A single sort call site that alternates between small (length <= 16,
// inlined insertion sort) and large (length > 16, inlined slow-path
// DirectCall to arrayProtoFuncSort) arrays must compile once and stay
// compiled. The dispatcher inside the intrinsic branches on runtime length
// so neither size should trigger an OSR exit at the dispatcher.
//
// If the dispatcher were broken (e.g. the slow path triggered BadIndexingType
// / BadType / OutOfBounds when seeing an unexpected size), this loop would
// repeatedly tier-down to baseline and recompile. We assert via $vm.ftlTrue()
// that the call site stays in FTL once it gets there.
//
// Hot loop runs inside `harness` rather than at <global> scope so:
//   (a) ftlTrue() reflects harness's tier directly, and
//   (b) heavyweight error-path code (correctness check) is moved out of the
//       inner loop, where it would otherwise pollute the tier with its own
//       OSR exits.
// Arrays are constructed via `new Array(N)` + indexed assignment so every
// iteration's argument has identical structure (no `.slice()` profiling
// noise that could trigger a CheckStructure churn at the intrinsic's entry).

function cmp(a, b) { return a - b; }
noInline(cmp);

const SMALL_LEN = 16;
const LARGE_LEN = 32;

const SMALL_SEED = [5, 3, 8, 1, 7, 2, 4, 6, 0, 9, 12, 14, 11, 13, 10, 15];

function makeSmall() {
    const a = new Array(SMALL_LEN);
    for (let i = 0; i < SMALL_LEN; i++) a[i] = SMALL_SEED[i];
    return a;
}

function makeLarge() {
    const a = new Array(LARGE_LEN);
    for (let i = 0; i < LARGE_LEN; i++) a[i] = LARGE_LEN - 1 - i;
    return a;
}

// Returns [inFTLCount, accumulator]. The accumulator sums each sorted array's
// first element. For both sizes that's 0, so the final accumulator must be
// exactly 0 (a cheap correctness sanity check that doesn't perturb the tier).
function harness(N) {
    let inFTL = 0;
    let acc = 0;
    for (let i = 0; i < N; i++) {
        let a;
        if (i & 1) {
            a = new Array(SMALL_LEN);
            for (let j = 0; j < SMALL_LEN; j++) a[j] = SMALL_SEED[j];
        } else {
            a = new Array(LARGE_LEN);
            for (let j = 0; j < LARGE_LEN; j++) a[j] = LARGE_LEN - 1 - j;
        }
        a.sort(cmp);
        if ($vm.ftlTrue()) inFTL++;
        acc += a[0];
    }
    return [inFTL, acc];
}

// Warm up: alternate sizes from the start so the DFG/FTL profile sees both
// shapes during compilation. A broken dispatcher would already have
// re-OSR'd into baseline several times by the end of warm-up.
harness(testLoopCount * 10);

// Steady state: by now harness is FTL-compiled (sort intrinsic + dispatcher
// fully inlined). Run a long stretch of mixed sizes and assert ftlTrue() is
// observed for every iteration, i.e. we are not bouncing between baseline
// and FTL.
const [inFTLCount, acc] = harness(testLoopCount);

if (acc !== 0)
    throw new Error("sort produced wrong leading element across iterations: acc=" + acc);

// In the steady state the harness must spend essentially all its time in
// FTL. Anything below 95% indicates repeated tier-down, the bug we are
// guarding against. Skip when FTL is not enabled: `ftlTrue()` is always false
// under --useFTLJIT=false / no-ftl / dfg-eager / mini-mode / lockdown etc.
if ($vm.useFTLJIT() && inFTLCount * 100 < testLoopCount * 95) {
    throw new Error("ArraySortIntrinsic appears to be tiering down repeatedly: "
        + "in FTL only " + inFTLCount + " / " + testLoopCount
        + " steady-state calls (expected >=95%)");
}

// Also do a one-shot full correctness check, independent of the hot loop,
// to make sure both small and large really do sort correctly.
function check(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error("length mismatch " + actual.length + " vs " + expected.length);
    for (let i = 0; i < expected.length; i++)
        if (actual[i] !== expected[i])
            throw new Error("at " + i + " got " + actual[i] + " expected " + expected[i]);
}

const small = makeSmall();
small.sort(cmp);
check(small, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);

const large = makeLarge();
large.sort(cmp);
{
    const expected = new Array(LARGE_LEN);
    for (let i = 0; i < LARGE_LEN; i++) expected[i] = i;
    check(large, expected);
}
