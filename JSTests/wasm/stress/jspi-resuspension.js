//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// The different test modules test resuspension at different depths in the Wasm stack.

let depth1 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (import "env" "get_number2" (func $get_number2 (result i32)))
  (func $c (result i32)
    i32.const 500
    call $get_number
    i32.const 500
    call $get_number2
    i32.add
    i32.add
    i32.add
  )
  (func $b (result i32)
    call $c
    i32.const 100
    i32.add
  )
  (func $a (export "entry") (result i32)
    call $b
    i32.const 50
    i32.add
    i32.const 1000
    i32.sub
  )
)`;

let depth2 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (import "env" "get_number2" (func $get_number2 (result i32)))
  (func $c (result i32)
    call $get_number
  )
  (func $b (result i32)
    call $c
    i32.const 100
    call $get_number2
    i32.add
    i32.add
  )
  (func $a (export "entry") (result i32)
    call $b
    i32.const 50
    i32.add
  )
)`;

let depth3 = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (import "env" "get_number2" (func $get_number2 (result i32)))
  (func $c (result i32)
    call $get_number
  )
  (func $b (result i32)
    call $c
    i32.const 100
    i32.add
  )
  (func $a (export "entry") (result i32)
    call $b
    i32.const 50
    i32.add
    call $get_number2
    i32.add
  )
)`;

let depth3inverted = `
(module
  (import "env" "get_number" (func $get_number (result i32)))
  (import "env" "get_number2" (func $get_number2 (result i32)))
  (func $c (result i32)
    call $get_number
  )
  (func $b (result i32)
    call $c
    i32.const 100
    i32.add
  )
  (func $a (export "entry") (result i32)
    call $get_number2
    i32.const 50
    call $b
    i32.add
    i32.add
  )
)`;

async function asyncReturn42() {
    return 42;
}

async function asyncReturn67() {
    return 67;
}

async function test(watSource) {
  const instance = await instantiate(watSource, {
    env: {
      get_number: new WebAssembly.Suspending(asyncReturn42),
      get_number2: new WebAssembly.Suspending(asyncReturn67)
    }
  });

  const probe = WebAssembly.promising(instance.exports.entry);

  for (let i = 0; i < wasmTestLoopCount; i++) {
    assert.eq(await probe(), 259)
  }
}

await test(depth1);
await test(depth2);
await test(depth3);
await test(depth3inverted);
