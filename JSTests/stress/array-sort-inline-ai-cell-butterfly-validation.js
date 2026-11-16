//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.1", "--validateAbstractInterpreterState=1", "--validateDFGClobberize=1")

// FTL ArraySortCompact returns a JSCellButterfly (SpecCellOther), not a
// JSObject. The DFG abstract interpreter (and prediction propagation) used to
// claim SpecObjectOther for this node, which is a disjoint, wrong type proof.
// With --validateAbstractInterpreterState=1 the FTL probe traps deterministically.
// This test ensures the AI/prediction match the runtime cell type so no probe fires.

function sortIt(a, cmp) { return a.sort(cmp); }
noInline(sortIt);

let cmp = (x, y) => x.idx - y.idx;
let template = [];
for (let i = 0; i < 8; i++) template.push({ idx: i });

for (let i = 0; i < testLoopCount; i++)
    sortIt(template.slice(), cmp);
