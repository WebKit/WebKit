//@ $skipModes << :lockdown
//@ requireOptions("--useExecutableAllocationFuzz=false")

// Microbenchmark for WebAssembly.Table construction (bug 288529).
const size = 100000;
const iterations = 20;
let sink = 0;
for (let i = 0; i < iterations; ++i) {
    const t1 = new WebAssembly.Table({ element: "externref", initial: size });
    const t2 = new WebAssembly.Table({ element: "funcref", initial: size });
    const t3 = new WebAssembly.Table({ element: "externref", initial: size }, {});
    sink += t1.length + t2.length + t3.length;
}
if (sink !== iterations * size * 3)
    throw new Error("bad sink " + sink);
