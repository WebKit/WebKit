//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useOMGJIT=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (memory (export "memory") i64 2)
    (data "hello")
    (func $testInit (export "testInit")
      (memory.init 0
        (i64.const 0)
        (i32.const 0)
        (i32.const 5))
    )
    (func $testFill (export "testFill") (param $dest i64) (param $byte i32) (param $sz i64)
      (memory.fill 
        (local.get $dest) 
        (local.get $byte) 
        (local.get $sz))
    )
    (func $testCopy (export "testCopy") (param $dest i64) (param $src i64) (param $sz i64)
      (memory.copy 
        (local.get $dest) 
        (local.get $src) 
        (local.get $sz))
    )
)
`;

const helloBytes = "hello";
const instance = await instantiate(wat, {}, {memory64: true});
const {testInit, testFill, testCopy, memory} = instance.exports;
const len = helloBytes.length;
const iterable = new DataView(memory.buffer);

function test() {
  // Write "hello" to 0 and verify
  testInit();
  for (let i = 0; i < len * 2; i++) {
    const b = iterable.getInt8(i);
    if (i < len)
      assert.eq(b, helloBytes.charCodeAt(i % len));
    else if (i < len * 2)
      assert.eq(b, 0);
  }

  // Copy "hello" to index 5 and verify "hello" appears twice
  testCopy(BigInt(len), 0n, BigInt(len));
  for (let i = 0; i < len * 2; i++) {
    const b = iterable.getInt8(i);
    assert.eq(b, helloBytes.charCodeAt(i % len));
  }

  // Fill memory with 0 and verify all 0s
  testFill(0n, 0, BigInt(len * 2));
  for (let i = 0; i < len * 2; i++) {
    const b = iterable.getInt8(i);
    assert.eq(b, 0);
  }
}

for (let i = 0; i < wasmTestLoopCount; i++)
    test();

function testOverflow() {
  // memory.fill: dstAddress + count overflows uint64
  assert.throws(() => testFill(0xffffffffffffffffn, 0, 1n),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");

  assert.throws(() => testFill(1n, 0, 0xffffffffffffffffn),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");

  // memory.copy: dstAddress + count overflows uint64
  assert.throws(() => testCopy(0xffffffffffffffffn, 0n, 1n),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");

  // memory.copy: srcAddress + count overflows uint64
  assert.throws(() => testCopy(0n, 0xffffffffffffffffn, 1n),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");

  assert.throws(() => testCopy(0n, 1n, 0xffffffffffffffffn),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");
}

for (let i = 0; i < wasmTestLoopCount; i++)
    testOverflow();

let i32Wat = `
(module
    (memory (export "mem") 1)
    (data (i32.const 0) "world!")
    (func $i32Copy (export "i32Copy") (param $dst i32) (param $src i32) (param $sz i32)
      (memory.copy
        (local.get $dst)
        (local.get $src)
        (local.get $sz))
    )
)
`;

const worldBytes = "world!";
const i32Instance = await instantiate(i32Wat, {}, {});
const { i32Copy, mem: i32Mem } = i32Instance.exports;
const i32MemView = new DataView(i32Mem.buffer);

function testI32Copy() {
  i32Copy(16, 0, worldBytes.length);
  for (let i = 0; i < worldBytes.length; i++)
    assert.eq(i32MemView.getInt8(16 + i), worldBytes.charCodeAt(i));
}

for (let i = 0; i < wasmTestLoopCount; i++)
    testI32Copy();

function testI32CopyOOB() {
  assert.throws(() => i32Copy(0, 0xfffffff0 | 0, 32),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");

  assert.throws(() => i32Copy(0, -1, 1),
      WebAssembly.RuntimeError,
      "Out of bounds memory access");
}

for (let i = 0; i < wasmTestLoopCount; i++)
    testI32CopyOOB();
