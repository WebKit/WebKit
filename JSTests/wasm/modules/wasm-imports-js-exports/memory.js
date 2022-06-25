export let memory = new WebAssembly.Memory({
    initial: 1,
});

export function setMemory(mem) {
    memory = mem;
}

const b = new Int8Array(memory.buffer);
b[0] = 42;
