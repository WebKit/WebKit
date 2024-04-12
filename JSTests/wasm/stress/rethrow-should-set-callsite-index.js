import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
    (type (func))
    (tag $exc (type 0))
    (func $empty)
    (func $rethrower
        try
            call $empty
            throw $exc
        catch_all
            rethrow 0
        end
    )
    (func $call-rethrower
        call $rethrower
    )
    (export "call_rethrower" (func $call-rethrower))
)
`;

async function test() {
    let instance = await instantiate(wat, {}, {exceptions: true});

    let caughtCount = 0;
    for (let i = 0; i < 10000; i ++) {
        try {
            instance.exports.call_rethrower();
        } catch (e) {
            assert.instanceof(e, WebAssembly.Exception);
            caughtCount ++;
        }
    }

    assert.eq(caughtCount, 10000);
}

assert.asyncTest(test());
