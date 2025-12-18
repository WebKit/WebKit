import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// test bulk linear memory instructions with multiple memories

let wat = `
(module
  (memory (export "memory0") 2 3)
  (memory (export "memory1") 2 4)

  (func (export "size0") (result i32) (memory.size 0))
  (func (export "size1") (result i32) (memory.size 1))

  (data (memory 0) (i32.const 0x1000) "\x01\x02\x03\x04\x05\x06\x07\x08")
  (data (memory 1) (i32.const 0x1000) "\x80\x90\xA0\xB0\xC0\xD0\xE0\xF0")
)
`

async function test() {
    const result = await instantiate(wat, {}, { multi_memory: true });
    const mem0 = result.instance.exports.memory;
    console.log(mem0);
}

await assert.asyncTest(test())
