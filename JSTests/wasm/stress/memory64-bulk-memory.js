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
