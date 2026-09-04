//@ requireOptions("--useWebAssemblyESMIntegration=true")
import * as constant from "./constant.wasm"
import * as sum from "./sum.wasm"
import * as assert from '../assert.js';

const instance = WebAssembly.namespaceInstance(constant);
assert.instanceof(instance, WebAssembly.Instance);
assert.eq(WebAssembly.namespaceInstance(constant), instance);

const sumInstance = WebAssembly.namespaceInstance(sum);
assert.instanceof(sumInstance, WebAssembly.Instance);
assert.eq(sumInstance === instance, false);
assert.eq(sumInstance.exports.sum(1, 2), 3);

assert.throws(() => {
    WebAssembly.namespaceInstance({});
}, TypeError, `WebAssembly.namespaceInstance() expects a WebAssembly module namespace object`);
assert.throws(() => {
    WebAssembly.namespaceInstance(null);
}, TypeError, `WebAssembly.namespaceInstance() expects a WebAssembly module namespace object`);
assert.throws(() => {
    WebAssembly.namespaceInstance(assert);
}, TypeError, `WebAssembly.namespaceInstance() expects a WebAssembly module namespace object`);
