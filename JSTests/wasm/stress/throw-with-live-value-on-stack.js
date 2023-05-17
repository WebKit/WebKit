import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (global i32 i32.const 42)
    (tag (param i32))

    (func (export "test") (param i32) (result i32)
        global.get 0
        local.get 0
        try (param i32)
            throw 0
        catch 0
            br 0
        end
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true });
    const { test } = instance.exports;
    assert.eq(test(41), 42);
}

assert.asyncTest(test());
