//@ requireOptions("--useExecutableAllocationFuzz=false")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Bug 319894 / 314444: i32 extended const results used as data/elem offsets (and globals)
// must behave as mod 2^32. Typed i32 consumers must not treat the full uint64 value as the offset.

const extended = { extended_const: true };

{
    // 4 * 0x40000000 = 2^32 -> offset 0; active data should write at the start of memory.
    let instance = await instantiate(`
    (module
      (memory (export "memory") 1)
      (data (offset (i32.mul (i32.const 4) (i32.const 0x40000000))) "A"))
    `, {}, extended);
    assert.eq(new Uint8Array(instance.exports.memory.buffer)[0], 0x41);
}

{
    // 65536 * 65536 = 2^32 -> 0.
    let instance = await instantiate(`
    (module
      (memory (export "memory") 1)
      (data (offset (i32.mul (i32.const 65536) (i32.const 65536))) "B"))
    `, {}, extended);
    assert.eq(new Uint8Array(instance.exports.memory.buffer)[0], 0x42);
}

{
    let instance = await instantiate(`
    (module
      (global (export "mul") i32 (i32.mul (i32.const 4) (i32.const 0x40000000)))
      (global (export "add") i32 (i32.add (i32.const 0x7fffffff) (i32.const 1)))
      (global (export "sub") i32 (i32.sub (i32.const 0) (i32.const 1)))
      (global (export "chain") i32
        (i32.add
          (i32.mul (i32.const 0x40000000) (i32.const 4))
          (i32.const 7))))
    `, {}, extended);
    assert.eq(instance.exports.mul.value, 0);
    assert.eq(instance.exports.add.value, -2147483648);
    assert.eq(instance.exports.sub.value, -1);
    assert.eq(instance.exports.chain.value, 7);
}

{
    let instance = await instantiate(`
    (module
      (table (export "table") 1 funcref)
      (elem (offset (i32.mul (i32.const 4) (i32.const 0x40000000))) $f)
      (func $f (export "f") (result i32) (i32.const 42)))
    `, {}, extended);
    assert.eq(instance.exports.table.get(0)(), 42);
}

print("const-expr-i32-wrap: ok");
