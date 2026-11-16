//@ $skipModes << :lockdown
//@ requireOptions("--useExecutableAllocationFuzz=false")
// Source in wasm/stress/memcpy-wasm

function eq(a, b) {
    if (a !== b)
        throw new Error("Not equal: " + a + " " + b);
}

let pages = 64
let memory = new WebAssembly.Memory({initial: pages, maximum: pages});

let i32 = new Int32Array(memory.buffer);
for (let i = 0; i < 100000; i++) {
    i32[i] = i;
}

try {
    const $1 = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
    0,97,115,109,1,0,0,0,1,7,1,96,3,127,127,127,0,2,12,1,2,106,115,3,109,101,109,2,1,pages,pages,3,2,1,0,6,1,0,7,13,1,9,100,111,95,109,101,109,99,112,121,0,0,10,57,1,55,1,1,127,65,0,33,3,3,64,2,64,32,2,32,3,70,13,0,32,1,65,4,108,32,3,65,4,108,106,32,0,32,3,65,4,108,106,40,0,0,54,0,0,32,3,65,1,106,33,3,12,1,11,11,11
    ])), { js: { mem: memory } });

    for (let i=0; i<500; ++i)
        $1.exports.do_memcpy(0,50000,40000);

    for (let i = 0; i < 50000; i++) {
        eq(i32[i], i);
    }
    for (let i = 50000; i < 50000+40000; i++) {
        eq(i32[i], i-50000);
    }
    for (let i = 50000+40000; i < 100000; i++) {
        eq(i32[i], i);
    }
} catch (e) {
    if (jscOptions().useExecutableAllocationFuzz === false)
        throw e
}
