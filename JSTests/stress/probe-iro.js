//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0")

// Sanity-check the $vm.probe → DebugProbe → IRO dump plumbing: the dump should
// report each probe id and IRO's identity fact for it.
load("./resources/iro-test-helpers.js", "caller relative");

function fn(idxBoost, x) {
    if (x > -100 && x < 0) {
        const r = $vm.probe("bitor", idxBoost | x);
        const z = $vm.probe("add", r + 100);
        return z;
    }
    return 0;
}
noInline(fn);

for (let i = 0; i < testLoopCount; i++) fn(7, -5);
const iro = makeIROHelper(fn);

if (!iro.probes.has("bitor")) throw new Error('probe "bitor" missing from dump');
if (!iro.probes.has("add")) throw new Error('probe "add" missing from dump');

// IRO models each DebugProbe with an identity relationship to itself, so a
// recorded self-fact confirms the probed node reached IRO and was tracked.
iro.assertRel({ at: "bitor", lhs: "bitor", rel: "==", rhs: "bitor" });
iro.assertRel({ at: "add", lhs: "add", rel: "==", rhs: "add" });
