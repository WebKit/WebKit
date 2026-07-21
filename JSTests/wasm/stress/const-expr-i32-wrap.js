import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

{
    let instance = await instantiate(`
    (module
      (memory (export "memory") 1)
      (data (offset (i32.mul (i32.const 4) (i32.const 0x40000000))) "A"))
    `);
    assert.eq(new Uint8Array(instance.exports.memory.buffer)[0], 0x41);
}

{
    let instance = await instantiate(`
    (module
      (memory (export "memory") 1)
      (data (offset (i32.mul (i32.const 65536) (i32.const 65536))) "B"))
    `);
    assert.eq(new Uint8Array(instance.exports.memory.buffer)[0], 0x42);
}

{
    let instance = await instantiate(`
    (module
      (global (export "mul") i32 (i32.mul (i32.const 4) (i32.const 0x40000000)))
      (global (export "add") i32 (i32.add (i32.const 0x7fffffff) (i32.const 1)))
      (global (export "sub") i32 (i32.sub (i32.const 0) (i32.const 1))))
    `);
    assert.eq(instance.exports.mul.value, 0);
    assert.eq(instance.exports.add.value, -2147483648);
    assert.eq(instance.exports.sub.value, -1);
}

{
    let instance = await instantiate(`
    (module
      (table (export "table") 1 funcref)
      (elem (offset (i32.mul (i32.const 4) (i32.const 0x40000000))) $f)
      (func $f (export "f") (result i32) (i32.const 42)))
    `);
    assert.eq(instance.exports.table.get(0)(), 42);
}
