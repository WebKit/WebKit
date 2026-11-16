//@ skip if $addressBits <= 32
//@ requireOptions("--useWasmFastMemory=1")

import * as assert from "../assert.js";
import { compile, instantiate } from "../wabt-wrapper.js";

let wat = `
    (module
        (import "foo" "mem" (memory $m 1))
        (func (export "test") (param i32)
            (select (i32.const 0) (i32.const ${1 << 31}) (local.get 0))
            i32.const 1
            i32.store
        )
    )
`;

let signalingMemory = createWebAssemblyMemoryWithMode({ initial: 1 }, "Signaling");
assert.truthy(signalingMemory instanceof WebAssembly.Memory);
let boundsCheckedMemory = createWebAssemblyMemoryWithMode({ initial: 1 }, "BoundsChecking");
assert.truthy(WebAssemblyMemoryMode(boundsCheckedMemory) == "BoundsChecking");

let wasmModule = await compile(wat);
let signalingInstance = new WebAssembly.Instance(wasmModule, { foo: { mem: signalingMemory }});
let boundsCheckedInstance = new WebAssembly.Instance(wasmModule, { foo: { mem: boundsCheckedMemory }});

for (let i = 0; i < 1e5; ++i)
    signalingInstance.exports.test(1);

assert.throws(boundsCheckedInstance.exports.test, WebAssembly.RuntimeError, "Out of bounds memory access (evaluating 'func(...args)')", 0);