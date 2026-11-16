//@ runDefault("--useDollarVM=1", "--thresholdForOptimizeAfterWarmUp=100")
//@ runDefault("--useDollarVM=1", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=100")

// The DFG/FTL code generators for RegExpTestInline must not depend on the RegExp's
// compilation state: deleteAllCode() can reset it on the main thread between strength
// reduction and code generation of the same compilation plan. The varying warm-up
// count is load-bearing: it sweeps the deleteAllCodeWhenIdle() timing across the
// compilation window (a fixed count does not reproduce the race).
//
// Phase 1 (tick) races DFG plans: each turn compiles a fresh function and deletes all
// code. Phase 2 (ftlTick) races FTL plans: a persistent function climbs the tiers
// between deletes (a per-turn function never executes its DFG code, so it can never
// request FTL), and deleting only every 5th turn leaves it time to get there.

let iteration = 0;
function tick() {
    let f = new Function("s", "return /ab+c/.test(s);");
    noInline(f);
    let n = 100 + ((iteration * 37) % 400);
    for (let i = 0; i < n; i++) {
        if (!f("xxabbbcyy"))
            throw new Error("expected match at iteration " + iteration);
    }
    $vm.deleteAllCodeWhenIdle();
    if (++iteration < 400)
        setTimeout(tick, 0);
    else
        setTimeout(ftlTick, 0);
}

let hot = function(s) { return /ab+c/.test(s); };
noInline(hot);
let ftlIteration = 0;
function ftlTick() {
    let n = 100 + ((ftlIteration * 37) % 400);
    for (let i = 0; i < n; i++) {
        if (!hot("xxabbbcyy"))
            throw new Error("expected match at FTL iteration " + ftlIteration);
    }
    if (ftlIteration % 5 === 4)
        $vm.deleteAllCodeWhenIdle();
    if (++ftlIteration < 400)
        setTimeout(ftlTick, 0);
}
setTimeout(tick, 0);
