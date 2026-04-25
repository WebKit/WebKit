import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let watExpressionStackBug = `
(module
    (import "env" "thrower" (func $thrower))
    (import "env" "val1" (global $val1 i32))
    (import "env" "val2" (global $val2 f64))
    (import "env" "val3" (global $val3 i32))
    (import "env" "tag" (tag $exc))

    (func (export "test") (result i32)
        (local $outerlocal i32)
        (local $innerlocal i32)

        global.get $val3  ;; 50
        local.set $outerlocal

        try $outer
            call $thrower
        catch $exc
            global.get $val1  ;; 100 (i32)
            global.get $val2  ;; 200.0 (f64)

            ;; Enter inner try with those values on the stack
            try $inner
                i32.const 25
                local.set $innerlocal
                call $thrower
            catch $exc
                ;; Inner catch completes - values 100 (i32), 200.0 (f64) should still be on stack
                ;; And innerlocal should still be 25
            end

            ;; Convert f64 to i32 and add: i32.trunc_f64_s(200.0) + 100 = 200 + 100 = 300
            i32.trunc_f64_s
            i32.add

            ;; Add the local values: 300 + 50 + 25 = 375
            local.get $outerlocal
            i32.add
            local.get $innerlocal
            i32.add
            return
        end

        ;; Should not reach here
        i32.const 999
    )
)
`;

let watExpressionStackBugWithOSR = `
(module
    (import "env" "thrower" (func $thrower))
    (import "env" "val1" (global $val1 i32))
    (import "env" "val2" (global $val2 f64))
    (import "env" "val3" (global $val3 i32))
    (import "env" "tag" (tag $exc))

    (func (export "test") (param $loopCount i32) (result i32)
        (local $outerlocal i32)
        (local $innerlocal i32)
        (local $counter i32)

        global.get $val3  ;; 50
        local.set $outerlocal

        try $outer
            call $thrower
        catch $exc
            global.get $val1  ;; 100 (i32)
            global.get $val2  ;; 200.0 (f64)

            try $inner
                i32.const 25
                local.set $innerlocal
                call $thrower
            catch $exc
                ;; Inner catch - do a hot loop to trigger OSR tier-up
                ;; This tests that expression stack and locals are preserved across OSR
                i32.const 0
                local.set $counter

                (block $break
                    (loop $continue
                        ;; Increment counter
                        local.get $counter
                        i32.const 1
                        i32.add
                        local.set $counter

                        local.get $counter
                        local.get $loopCount
                        i32.lt_u
                        br_if $continue
                    )
                )
                ;; After OSR, values 100 (i32), 200.0 (f64) should still be on stack
                ;; And innerlocal should still be 25
            end

            ;; Convert f64 to i32 and add: i32.trunc_f64_s(200.0) + 100 = 200 + 100 = 300
            i32.trunc_f64_s
            i32.add

            ;; Add the local values: 300 + 50 + 25 = 375
            local.get $outerlocal
            i32.add
            local.get $innerlocal
            i32.add
            return
        end

        ;; Should not reach here
        i32.const 999
    )
)
`;

async function testExpressionStack() {
    const tag = new WebAssembly.Tag({ parameters: [] });

    const importObject = {
        env: {
            tag: tag,
            thrower: () => { throw new WebAssembly.Exception(tag, []); },
            val1: new WebAssembly.Global({ value: 'i32', mutable: false }, 100),
            val2: new WebAssembly.Global({ value: 'f64', mutable: false }, 200.0),
            val3: new WebAssembly.Global({ value: 'i32', mutable: false }, 50)
        }
    };

    const instance = await instantiate(watExpressionStackBug, importObject, { exceptions: true });

    for (let i = 0; i < wasmTestLoopCount; i++) {
        let result = instance.exports.test();
        // Expected: i32.trunc_f64_s(200.0) + 100 + 50 + 25 = 200 + 100 + 50 + 25 = 375
        assert.eq(result, 375);
    }
}

async function testExpressionStackWithOSR() {
    const tag = new WebAssembly.Tag({ parameters: [] });

    const importObject = {
        env: {
            tag: tag,
            thrower: () => { throw new WebAssembly.Exception(tag, []); },
            val1: new WebAssembly.Global({ value: 'i32', mutable: false }, 100),
            val2: new WebAssembly.Global({ value: 'f64', mutable: false }, 200.0),
            val3: new WebAssembly.Global({ value: 'i32', mutable: false }, 50)
        }
    };

    const instance = await instantiate(watExpressionStackBugWithOSR, importObject, { exceptions: true });

    // Use a large loop count to ensure OSR tier-up happens
    const loopCount = 10000;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        let result = instance.exports.test(loopCount);
        // Expected: i32.trunc_f64_s(200.0) + 100 + 50 + 25 = 200 + 100 + 50 + 25 = 375
        assert.eq(result, 375);
    }
}

assert.asyncTest(testExpressionStack());
assert.asyncTest(testExpressionStackWithOSR());
