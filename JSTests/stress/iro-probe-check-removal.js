//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// The dump keys each eliminated op by the probe pinning the value it acted on,
// so asserting on that per-probe record is robust to node renumbering and
// unrelated ops, unlike a graph-wide op count or an IR-text grep.
//
// Pin the index with a probe but index with the raw value: using a DebugProbe
// directly as an array subscript destabilizes the access and blocks FTL.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(arr, i) {
    let s = 0;
    if (i >= 0 && i < arr.length) {
        $vm.probe("guarded", i);     // IRO proves arr[i] in-bounds -> check eliminated
        s += arr[i] | 0;
    }
    $vm.probe("unguarded", i & 7);   // valid at runtime, unprovable < length -> check kept
    s += arr[i & 7] | 0;
    return s;
}
noInline(fn);

const a = [1, 2, 3, 4, 5, 6, 7, 8];
for (let k = 0; k < testLoopCount; k++) fn(a, k & 7);
const iro = makeIROHelper(fn);

iro.assertEliminated("CheckInBounds", "guarded");
iro.assertNotEliminated("CheckInBounds", "unguarded");
