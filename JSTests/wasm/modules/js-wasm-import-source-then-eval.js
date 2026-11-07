//@ requireOptions("--useSourcePhaseImports=true")
import source wasmModule from "./constant.wasm"
import { constant } from "./constant.wasm"
import * as wasmNS from "./constant.wasm"
import * as assert from '../assert.js';

assert.eq(wasmModule instanceof WebAssembly.Module, true);
assert.eq(constant, 42);
assert.eq(wasmNS.constant, 42);

const instance = new WebAssembly.Instance(wasmModule);
assert.eq(instance.exports.constant.value, 42);

const ns = await import("./constant.wasm");
assert.eq(ns.constant, 42);

const identity = await import("./resources/js-wasm-import-source-identity.js");
assert.eq(identity.mod1, identity.mod2);
assert.eq(identity.mod3, identity.mod4);
assert.eq(identity.mod3, wasmModule);
