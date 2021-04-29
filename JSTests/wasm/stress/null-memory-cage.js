import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "test") (param $sz i32) (result i32)
        (local.get $sz)
    )
)
`;
async function test() {
    const instance = await instantiate(wat, {}, {reference_types: true});
    const {test} = instance.exports;
    assert.eq(test(42), 42);
}

assert.asyncTest(test());
