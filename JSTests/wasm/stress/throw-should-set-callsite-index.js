import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
    (type (func))
    (tag $exc (type 0))
    (func $empty)
    (func $thrower
        try
            call $empty
        catch_all
        end
        throw $exc
    )
    (func $call-thrower
        call $thrower
    )
    (export "call_thrower" (func $call-thrower))
)
`;

async function test() {
    let instance = await instantiate(wat, {}, {exceptions: true});

    let caughtCount = 0;
    for (let i = 0; i < 10000; i ++) {
        try {
            instance.exports.call_thrower();
        } catch (e) {
            assert.instanceof(e, WebAssembly.Exception);
            caughtCount ++;
        }
    }

    assert.eq(caughtCount, 10000);
}

assert.asyncTest(test());
