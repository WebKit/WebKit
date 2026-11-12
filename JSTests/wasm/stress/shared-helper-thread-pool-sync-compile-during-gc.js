//@ requireOptions("--useSharedHelperThreadPool=true", "--helperThreadPoolTargetCapacity=1", "--collectContinuously=true")

import * as assert from "../assert.js";
import { watToWasm } from "../wabt-wrapper.js";

// Synchronous compiles block the mutator without releasing heap access while GC marking helpers may hold
// every pool thread. The per-class minimum must still let the Wasm compile run.
async function test() {
    let functions = "";
    for (let i = 0; i < 200; ++i)
        functions += `(func (export "f${i}") (param i32) (result i32) (i32.add (local.get 0) (i32.const ${i})))\n`;
    let bytes = await watToWasm(`(module ${functions})`);

    for (let i = 0; i < 50; ++i) {
        assert.truthy(WebAssembly.validate(bytes));
        let instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
        assert.eq(instance.exports.f7(1), 8);
    }
}

await assert.asyncTest(test());
