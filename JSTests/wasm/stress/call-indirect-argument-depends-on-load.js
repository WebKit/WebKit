import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (type $takes_i32_i32 (func (param i32 i32) (result i32)))
    (table 1 funcref)
    (memory $mem 10 100)

    (func $foo (param i32 i32) (result i32)
        local.get 0
    )

    (elem (i32.const 0) $foo)

    (func (export "test") (result i32) (local i32)
        i32.const 0
        i32.const 42
        i32.store

        i32.const 0
        i32.load
        local.get 0
        i32.const 0
        call_indirect (type $takes_i32_i32)
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { test } = instance.exports;
    for (let i = 0; i < 10000; i ++)
        assert.eq(test(), 42);
}

await assert.asyncTest(test());
