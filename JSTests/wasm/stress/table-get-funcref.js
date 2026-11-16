import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
  (table $table 2 funcref)
  (elem (i32.const 1) $f)
  (func $f (result i32)
    (i32.const 42))
  (func (export "get") (param i32) (result funcref)
    (table.get $table (local.get 0)))
  (func (export "set") (param i32 funcref)
    (table.set $table (local.get 0) (local.get 1)))
  (func (export "call") (param i32) (result i32)
    (call_indirect (result i32) (local.get 0)))
  (func (export "grow") (param i32) (result i32)
    (table.grow $table (ref.null func) (local.get 0)))
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { reference_types: true });
    const { get, set, call, grow } = instance.exports;

    assert.eq(get(0), null);
    const f = get(1);
    assert.eq(typeof f, "function");
    assert.eq(f(), 42);
    assert.eq(get(1), f);
    assert.eq(call(1), 42);

    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(get(0), null);
        assert.eq(get(1), f);
        set(0, f);
        assert.eq(get(0), f);
        assert.eq(call(0), 42);
        set(0, null);
        assert.eq(get(0), null);
    }

    assert.eq(grow(1), 2);
    set(2, f);
    assert.eq(get(2), f);

    assert.throws(() => get(3), WebAssembly.RuntimeError, "Out of bounds table access (evaluating 'func(...args)')");
}

await assert.asyncTest(test());
