//@ skip if not $jitTests
//@ runDefault("--useConcurrentJIT=0")

function ifMod(x) { if (x % 3) return 1; return 0; }
noInline(ifMod);

function ifNotMod(x) { if (!(x % 3)) return 1; return 0; }
noInline(ifNotMod);

function ternaryMod(x) { return (x % 3) ? 1 : 0; }
noInline(ternaryMod);

function whileMod(x) { while (x % 3) x++; return x; }
noInline(whileMod);

for (let i = -testLoopCount; i < testLoopCount; ++i) {
    ifMod(i);
    ifNotMod(i);
    ternaryMod(i);
    whileMod(i);
}

for (let f of [ifMod, ifNotMod, ternaryMod, whileMod]) {
    if (numberOfDFGCompiles(f) > 2)
        throw new Error(f.name + " was compiled " + numberOfDFGCompiles(f) + " times");
}
