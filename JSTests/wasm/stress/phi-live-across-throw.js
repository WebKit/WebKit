import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (global i32 i32.const 43)
    (global i32 i32.const 1)
    (func (export "test") (result i32) (local i32)
        i32.const 42
        local.set 0
        block
            global.get 1
            br_if 0
            global.get 0
            local.set 0
        end
        try
            throw $exc
        catch $exc
        end
        local.get 0
    )
    (tag $exc)
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true });
    for (let i = 0; i < 20000; i ++)
        assert.eq(instance.exports.test(), 42);
}

assert.asyncTest(test());
