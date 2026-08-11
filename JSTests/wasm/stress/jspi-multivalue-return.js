//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test JSPI with multi-value returns from a suspending import.
// Exercises the WasmToJS thunk's result area when the slice bottom
// must account for the full stackOffset allocation.

let fiveI64 = `
(module
  (import "env" "s" (func $s (param i32) (result i64 i64 i64 i64 i64)))
  (func $entry (export "entry") (param $p0 i32) (result i32)
    local.get $p0
    call $s
    drop
    drop
    drop
    drop
    i32.wrap_i64
  )
)`;

let threeI64 = `
(module
  (import "env" "s" (func $s (param i32) (result i64 i64 i64)))
  (func $entry (export "entry") (param $p0 i32) (result i32)
    local.get $p0
    call $s
    drop
    drop
    i32.wrap_i64
  )
)`;

let mixedTypes = `
(module
  (import "env" "s" (func $s (param i32) (result f64 f32 i32 i64 f64 f32)))
  (func $entry (export "entry") (param $p0 i32) (result i32)
    local.get $p0
    call $s
    drop
    drop
    drop
    drop
    drop
    i32.trunc_sat_f64_s
  )
)`;

let deepStack = `
(module
  (import "env" "s" (func $s (param i32) (result i64 i64 i64 i64 i64)))
  (func $inner (param $p0 i32) (result i32)
    local.get $p0
    call $s
    drop
    drop
    drop
    drop
    i32.wrap_i64
  )
  (func $middle (param $p0 i32) (result i32)
    local.get $p0
    call $inner
    i32.const 100
    i32.add
  )
  (func $entry (export "entry") (param $p0 i32) (result i32)
    local.get $p0
    call $middle
    i32.const 200
    i32.add
  )
)`;

async function testFiveI64() {
    async function suspend(x) {
        return [BigInt(0x42), BigInt(0x43), BigInt(0x44), BigInt(0x45), BigInt(0x46)];
    }

    const instance = await instantiate(fiveI64, {
        env: { s: new WebAssembly.Suspending(suspend) }
    });
    const entry = WebAssembly.promising(instance.exports.entry);

    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(await entry(0), 0x42);
}

async function testThreeI64() {
    async function suspend(x) {
        return [BigInt(10), BigInt(20), BigInt(30)];
    }

    const instance = await instantiate(threeI64, {
        env: { s: new WebAssembly.Suspending(suspend) }
    });
    const entry = WebAssembly.promising(instance.exports.entry);

    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(await entry(0), 10);
}

async function testMixedTypes() {
    async function suspend(x) {
        return [7.0, 3.14, 99, BigInt(12345), 2.718, 1.5];
    }

    const instance = await instantiate(mixedTypes, {
        env: { s: new WebAssembly.Suspending(suspend) }
    });
    const entry = WebAssembly.promising(instance.exports.entry);

    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(await entry(0), 7);
}

async function testDeepStack() {
    async function suspend(x) {
        return [BigInt(50), BigInt(0), BigInt(0), BigInt(0), BigInt(0)];
    }

    const instance = await instantiate(deepStack, {
        env: { s: new WebAssembly.Suspending(suspend) }
    });
    const entry = WebAssembly.promising(instance.exports.entry);

    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(await entry(0), 350);
}

await testFiveI64();
await testThreeI64();
await testMixedTypes();
await testDeepStack();
