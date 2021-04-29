import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (memory $mem0 1)
    (func (export "test") (param $sz i32) (result i32)
        (i32.load offset=0xff00 (local.get $sz))
    )
)
`;
async function test() {
    const instance = await instantiate(wat, {}, {reference_types: true});
    const {test} = instance.exports;
    // In fast-memory configuration, this pass bound-checking (expected).
    // And then, accessing to redzone, and this causes fault. And signal handler throws an error correctly.
    assert.throws(() => {
        test(0xffffffff);
    }, WebAssembly.RuntimeError, `Out of bounds memory access`);
}

assert.asyncTest(test());
