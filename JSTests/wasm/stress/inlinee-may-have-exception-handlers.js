import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
    (func $main (export "main")
        call $catcher
        i64.const 3127057505886423800
        i64.const -5049743701649469475
        i32.const 1279394412
        i32.const 1249280136
        i32.const -851957055
        i32.div_s
        i32.div_s
        select (result i64)
        loop (result i64)
        i64.const 7052281334997434446
        f64.const 0x1.f0094d7063744p+967
        i64.trunc_sat_f64_u
        i64.rem_s
        end
        i64.add
        f64.convert_i64_s
        i32.const 993640798
        i32.const -291103156
        i32.atomic.rmw.xor offset=36014
        f32.const 0x1.2e5784p-38
        i64.const 8981315711995315489
        i64.const 5932917051412947299
        i64.const 6207621187208520631
        i32.const 1636976966
        select (result i64)
        i64.ne
        br 0
        i32.trunc_sat_f32_u
        i32.div_s
        f64.const 0x1.7c35ecf56d865p-116
        i32.const 1987656988
        i64.const -1947434429939530388
        f64.const -0x1.ca05a27b00c85p+372
        i64.const 5169929820610505455
        block (param f64 i32 f64 i32 i64 f64 i64)
        drop
        drop
        drop
        drop
        drop
        drop
        drop
        end
    )
    (func $empty (param i32))
    (func $catcher (type 0)
        try
        i32.const 1
        call $empty
        catch_all
        throw $exc
        end
        call $main
    )
    (memory 32 64)
    (tag $exc)
)
`;

async function test() {
    let instance = await instantiate(wat, {}, {threads: true, exceptions: true});

    let caughtCount = 0;
    for (let i = 0; i < 10; i ++) {
        try {
            instance.exports.main();
        } catch (e) {
            // We expect to either overflow the stack, or end up throwing $exc back into JavaScript.
            assert.truthy(e instanceof RangeError || e instanceof WebAssembly.Exception);
            caughtCount ++;
        }
    }

    assert.eq(caughtCount, 10);
}

assert.asyncTest(test());
