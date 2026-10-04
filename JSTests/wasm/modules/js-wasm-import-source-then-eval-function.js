//@ requireOptions("--useSourcePhaseImports=true")
import source sumModule from "./sum.wasm"
import { sum } from "./sum.wasm"
import source startModule from "./start.wasm"
import { get } from "./start.wasm"
import * as assert from '../assert.js';

assert.eq(sumModule instanceof WebAssembly.Module, true);
assert.eq(sum(32, 42), 74);

const instance = new WebAssembly.Instance(sumModule);
assert.eq(instance.exports.sum(1, 2), 3);

assert.eq(startModule instanceof WebAssembly.Module, true);
assert.eq(get(), 1);

const ns = await import("./sum.wasm");
assert.eq(ns.sum, sum);
