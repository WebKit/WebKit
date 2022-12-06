//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

async function buildAndThrow(text)
{
    let error = null;
    try {
        await instantiate(text, { }, { threads: true });
    } catch (e) {
        error = e;
    }
    return error;
}

async function testLoad()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.load") (param $addr i32) (result i32) (i32.atomic.load align=8 (local.get $addr)))
    )`;
    let error = await buildAndThrow(text);
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 6: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')`);
}

async function testStore()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.store") (param $addr i32) (param $value i32) (i32.atomic.store align=8 (local.get $addr) (local.get $value)))
    )`;
    let error = await buildAndThrow(text);
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 8: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')`);
}

async function testRMW()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.rmw.add") (param $addr i32) (param $value i32) (result i32) (i32.atomic.rmw.add align=8 (local.get $addr) (local.get $value)))
    )`;
    let error = await buildAndThrow(text);
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 8: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')`);
}

async function testCmpXchg()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "i32.atomic.rmw.cmpxchg") (param $addr i32) (param $expected i32) (param $value i32) (result i32) (i32.atomic.rmw.cmpxchg align=8 (local.get $addr) (local.get $expected) (local.get $value)))
    )`;
    let error = await buildAndThrow(text);
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 10: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')`);
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
    let error = await buildAndThrow(text);
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 8: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 1 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')`);
}

async function testAtomicWait()
{
    let text = `
    (module
      (memory 1 1 shared)
      (func (export "memory.atomic.wait32") (param $addr i32) (param $expected i32) (param $timeout i64) (result i32)
          (memory.atomic.wait32 align=8 (local.get 0) (local.get 1) (local.get 2)))
    )`;
    let error = await buildAndThrow(text);
    assert.eq(String(error), `CompileError: WebAssembly.Module doesn't parse at byte 10: byte alignment 8 does not match against atomic op's natural alignment 4, in function at index 0 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')`);
}

await testLoad();
await testStore();
await testRMW();
await testCmpXchg();
await testAtomicNotify();
await testAtomicWait();
