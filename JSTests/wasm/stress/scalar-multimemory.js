import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// test scalar instructions that access linear memory

let wat = `
(module
  (memory (export "memory0") 2 3)
  (memory (export "memory1") 2 4)

  (func (export "testloads") (param i32) (result i32)
    local.get 0
    i32.load8_s 0
  )
  (func (export "teststores") (param i32 i32)
    local.get 0
    local.get 1
    i32.store8 0
  )

  (data (memory 0) (i32.const 0x1000) "\x01\x02\x03\x04\x05\x06\x07\x08")
  (data (memory 1) (i32.const 0x1000) "\x80\x90\xA0\xB0\xC0\xD0\xE0\xF0")
)
`

// Test:
//
// {i32,i64,f32,f64}.load
// i32.load{8,16}_{s,u}
// i64.load{8,16,32}_{s,u}
//
// {i32,i64,f32,f64}.store
// i32.store{8,16}
// i64.store{8,16,32}

async function test() {
    const result = await instantiate(wat, {}, { multi_memory: true });
    const mem0 = result.instance.exports.memory;
    console.log(mem0);
}

await assert.asyncTest(test())
