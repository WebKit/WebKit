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
    // print(i);
    assert.eq(await runTest(), expected)
  }
}

await test(depth1, 92);
await test(depth2, 192);
await test(depth3, 392);
await test(depth4, 492);
