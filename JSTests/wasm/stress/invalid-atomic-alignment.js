//@ skip if $architecture != "arm64" && $architecture != "x86_64"  && !($architecture == "arm" && !$cloop)
import * as assert from '../assert.js';
import { watToWasm } from "../wabt-wrapper.js";

async function testLoad()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.load") (param $addr i32) (result i32) (i32.atomic.load align=8 (local.get $addr)))
    )`;
    await assert.compileErrorAsync(watToWasm(text, { threads: true }),
        `WebAssembly.Module doesn't parse at byte 6: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0`);
}

async function testStore()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.store") (param $addr i32) (param $value i32) (i32.atomic.store align=8 (local.get $addr) (local.get $value)))
    )`;
    await assert.compileErrorAsync(watToWasm(text, { threads: true }),
        `WebAssembly.Module doesn't parse at byte 8: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0`);
}

async function testRMW()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.rmw.add") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.add align=8 (local.get $addr) (local.get $value)))
    )`;
    await assert.compileErrorAsync(watToWasm(text, { threads: true }),
        `WebAssembly.Module doesn't parse at byte 8: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0`);
}

async function testCmpXchg()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.rmw.cmpxchg") (param $addr i32) (param $expected i32) (param $value i32) (result i32) (i32.atomic.rmw.cmpxchg align=8 (local.get $addr) (local.get $expected) (local.get $value)))
    )`;
    await assert.compileErrorAsync(watToWasm(text, { threads: true }),
        `WebAssembly.Module doesn't parse at byte 10: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0`);
}

async function testAtomicNotify()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "init") (param $value i64) (i64.store (i32.const 0) (local.get $value)))
      (func (export "memory.atomic.notify") (param $addr i32) (param $expected i32) (result i32)
          (memory.atomic.notify align=8 (local.get 0) (local.get 1)))
    )`;
    await assert.compileErrorAsync(watToWasm(text, { threads: true }),
        `WebAssembly.Module doesn't parse at byte 8: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 1`);
}

async function testAtomicWait()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "memory.atomic.wait32") (param $addr i32) (param $expected i32) (param $timeout i64) (result i32)
          (memory.atomic.wait32 align=8 (local.get 0) (local.get 1) (local.get 2)))
    )`;
    await assert.compileErrorAsync(watToWasm(text, { threads: true }),
        `WebAssembly.Module doesn't parse at byte 10: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0`);
}

await testLoad();
await testStore();
await testRMW();
await testCmpXchg();
await testAtomicNotify();
await testAtomicWait();
