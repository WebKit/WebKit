//@ $skipModes << :lockdown
//@ requireOptions("--useWasmFastMemory=false", "--useExecutableAllocationFuzz=false")

const iterations = 100;
const maxPages = 128;
let total = 0;
for (let i = 0; i < iterations; ++i) {
    const memory = new WebAssembly.Memory({ initial: 1, maximum: maxPages });
    for (let p = 1; p < maxPages; ++p)
        total += memory.grow(1);
}
const expected = iterations * (maxPages - 1) * maxPages / 2;
if (total !== expected)
    throw new Error("bad total: " + total + " expected " + expected);
