import { g } from "./entry-global.wasm"
export const globalFromJS = new WebAssembly.Global(
    { value: "i32", mutable: true },
    42
);
export function incrementGlobal() {
    g.value = g.value + 1;
}
