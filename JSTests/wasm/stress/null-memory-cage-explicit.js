import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (memory 0 0)
    (func (export "test") (param $sz i32) (result i32)
        (memory.grow (local.get $sz))
    )
)
`;
async function test() {
    const instance = await instantiate(wat, {}, {reference_types: true});
    const {test} = instance.exports;
    assert.eq(test(42), -1);
}

assert.asyncTest(test());
