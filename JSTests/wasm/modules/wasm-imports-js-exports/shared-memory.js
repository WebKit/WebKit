export const memory = new WebAssembly.Memory({
    initial: 1,
    maximum: 10,
    shared: true
});

const b = new Int8Array(memory.buffer);
b[0] = 42;
