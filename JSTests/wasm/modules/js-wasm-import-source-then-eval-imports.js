//@ requireOptions("--useSourcePhaseImports=true")
import source wasmModule from "./wasm-imports-js-exports/imports.wasm"
import { addOne } from "./wasm-imports-js-exports/imports.wasm"
import * as assert from '../assert.js';

assert.eq(wasmModule instanceof WebAssembly.Module, true);
assert.eq(addOne(32), 33);
assert.eq(addOne(-2), -1);

const pendingSource = import.source("./wasm-imports-js-exports/imports.wasm");
const ns = await import("./wasm-imports-js-exports/imports.wasm");
const dynamicSource = await pendingSource;
assert.eq(ns.addOne, addOne);
assert.eq(dynamicSource, wasmModule);
