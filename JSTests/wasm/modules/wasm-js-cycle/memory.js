import { m } from "./entry-memory.wasm"
export function setMemory(index, val) {
    const view = new Int32Array(m.buffer);
    view[index] = val;
}
