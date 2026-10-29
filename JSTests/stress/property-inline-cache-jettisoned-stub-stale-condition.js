//@ requireOptions("--forceEagerCompilation=1", "--maxAccessVariantListSize=64", "--alwaysUseShadowChicken=1")

let g = 1;
function probe(mode) {
    let t = g;                       // FTL code watches g's constant-inference watchpoint
    let arr = [1, 2, 3, 4, 5];
    let acc = 0;
    for (let i = 0; i < 4; i++)
        if (arr[Symbol("k")] !== undefined) acc++;   // symbol-keyed GetByVal miss
    if (mode) {
        g = 2;                       // jettisons probe's FTL CodeBlock while probe is running
        $vm.gc();                    // full GC + sync sweep: the symbols are freed
        Array.prototype[Symbol("z")] = 1;   // transition -> fires the stale watchpoint
    }
    return t + acc;
}
for (let i = 0; i < 500; i++) probe(0);
probe(1);
