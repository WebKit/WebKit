//@ requireOptions("--useWasmMemoryToBufferAPIs=true")

import * as assert from '../assert.js'

const memories = [];
for (let i = 0; i < 500; i++) {
    try {
        memories.push(new WebAssembly.Memory({ initial: 1, maximum: 100 }));
    } catch (e) { break; }
}

const memory = new WebAssembly.Memory({ initial: 1, maximum: 10 });
const buffer = memory.toResizableBuffer();
const ta = new Uint32Array(buffer);
ta[0] = 0xCAFEBABE;

memory.grow(1);

for (let i = 0; i < 100; i++) {
    const arr = new ArrayBuffer(65536);
    new Uint32Array(arr).fill(0xDEADBEEF);
}

assert.eq(ta[0], 0xCAFEBABE);
