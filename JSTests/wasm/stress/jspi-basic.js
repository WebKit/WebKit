//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Basic JSPI invocation: a suspension that then returns directly out of Wasm,
// for different depths of overall Wasm stack.

let depth1 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (func $a (export "entry") (result i32)
    i32.const 20
    call $get_number
    i32.const 30
    i32.add
    i32.add
  )
)`;

let depth2 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (func $z (result i32)
    i32.const 20
    call $get_number
    i32.const 30
    i32.add
    i32.add
  )
  (func $a (export "entry") (result i32)
    i32.const 60
    call $z
    i32.const 40
    i32.add
    i32.add
  )
)`;

let depth3 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (func $z (result i32)
    i32.const 20
    call $get_number
    i32.const 30
    i32.add
    i32.add
  )
  (func $b (result i32)
    i32.const 60
    call $z
    i32.const 40
    i32.add
    i32.add
  )
  (func $a (export "entry") (result i32)
    i32.const 120
    call $b
    i32.const 80
    i32.add
    i32.add
  )
)`;

let depth4 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (func $z (result i32)
    i32.const 20
    call $get_number
    i32.const 30
    i32.add
    i32.add
  )
  (func $c (result i32)
    i32.const 60
    call $z
    i32.const 40
    i32.add
    i32.add
  )
  (func $b (result i32)
    i32.const 30
    call $c
    i32.const 70
    i32.add
    i32.add
  )
  (func $a (export "entry") (result i32)
    i32.const 120
    call $b
    i32.const 80
    i32.add
    i32.add
  )
)`;

async function asyncReturn42() {
    return 42;
}

async function test(watSource, expected) {
  const instance = await instantiate(watSource, {
    env: {
      get_number: new WebAssembly.Suspending(asyncReturn42)
    }
  });
  const runTest = WebAssembly.promising(instance.exports.entry);

  for (let i = 0; i < wasmTestLoopCount; i++) {
    assert.eq(await runTest(), expected)
  }
}

await test(depth1, 92);
await test(depth2, 192);
await test(depth3, 392);
await test(depth4, 492);

// Loop with imported function call and sum accumulation
let loopTest = `
(module
  (import "env" "get_value" (func $get_value (param i32) (result i32)))
  (import "env" "print" (func $print (param i32)))
  (func $loop_and_sum (export "loop_and_sum") (result i32)
    (local $sum i32)
    (local $counter i32)

    ;; Initialize locals
    i32.const 0
    local.set $sum
    i32.const 0
    local.set $counter

    ;; Loop 10 times
    block $exit
      loop $continue
        ;; Exit the loop if counter >= 10
        local.get $counter
        ;; local.get $counter
        ;; call $print
        i32.const 10
        i32.ge_s
        br_if $exit

        ;; sum += get_value(counter)
        local.get $counter
        call $get_value
        local.get $sum
        i32.add
        local.set $sum

        ;; counter += 1
        local.get $counter
        i32.const 1
        i32.add
        local.set $counter

        br $continue
      end
    end

    ;; Return the accumulated sum
    local.get $sum
  )
)`;

let callCounter = 0;
async function asyncGetValue(expectedCounter) {
    assert.eq(callCounter, expectedCounter);
    return callCounter++;
}

async function testLoopAccumulation() {
    callCounter = 0; // Reset counter for this test

    const instance = await instantiate(loopTest, {
        env: {
            get_value: new WebAssembly.Suspending(asyncGetValue),
            print: print
        }
    });
    const runTest = WebAssembly.promising(instance.exports.loop_and_sum);

    const result = await runTest();

    // Assert that the loop executed exactly 10 times
    assert.eq(callCounter, 10);
    // Assert that the computed sum is correct (0+1+2+3+4+5+6+7+8+9 = 45)
    assert.eq(result, 45);
}

await testLoopAccumulation();

// Test with a function that has 24 parameters and verify argument preservation
// entry() calls add_all() with 24 arguments, add_all() calls call_out(),
// call_out() calls get_number() (suspends), adds 100, returns to add_all(),
// add_all() then adds all 24 parameters to the result
let manyParams = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (func $call_out (result i32)
    ;; Call get_number which suspends and returns 42, then add 100
    call $get_number
    i32.const 100
    i32.add
  )
  (func $add_all (param $p1 i32) (param $p2 i32) (param $p3 i32) (param $p4 i32)
                 (param $p5 i32) (param $p6 i32) (param $p7 i32) (param $p8 i32)
                 (param $p9 i32) (param $p10 i32) (param $p11 i32) (param $p12 i32)
                 (param $p13 i32) (param $p14 i32) (param $p15 i32) (param $p16 i32)
                 (param $p17 i32) (param $p18 i32) (param $p19 i32) (param $p20 i32)
                 (param $p21 i32) (param $p22 i32) (param $p23 i32) (param $p24 i32)
                 (result i32)
    ;; Call call_out which returns 42 + 100 = 142
    call $call_out
    ;; Add all 24 parameters to the result
    local.get $p1
    i32.add
    local.get $p2
    i32.add
    local.get $p3
    i32.add
    local.get $p4
    i32.add
    local.get $p5
    i32.add
    local.get $p6
    i32.add
    local.get $p7
    i32.add
    local.get $p8
    i32.add
    local.get $p9
    i32.add
    local.get $p10
    i32.add
    local.get $p11
    i32.add
    local.get $p12
    i32.add
    local.get $p13
    i32.add
    local.get $p14
    i32.add
    local.get $p15
    i32.add
    local.get $p16
    i32.add
    local.get $p17
    i32.add
    local.get $p18
    i32.add
    local.get $p19
    i32.add
    local.get $p20
    i32.add
    local.get $p21
    i32.add
    local.get $p22
    i32.add
    local.get $p23
    i32.add
    local.get $p24
    i32.add
  )
  (func $a (export "entry") (result i32)
    ;; Push 24 arguments: 1, 2, 3, ..., 24
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    call $add_all
  )
)`;

// Expected: (42 + 100) + (1+2+3+...+24) = 142 + 300 = 442
await test(manyParams, 442);
