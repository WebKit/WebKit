import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
  (table $table 1 externref)
  (func (export "get") (param i32) (result externref)
    (table.get $table (local.get 0)))
  (func (export "set") (param i32 externref)
    (table.set $table (local.get 0) (local.get 1)))
  (func (export "setNull") (param i32)
    (table.set $table (local.get 0) (ref.null extern)))
  (func (export "grow") (param i32) (result i32)
    (table.grow $table (ref.null extern) (local.get 0)))
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { reference_types: true });
    const { get, set, setNull, grow } = instance.exports;
    const obj = { x: 1 };

    assert.eq(get(0), null);
    for (let i = 0; i < wasmTestLoopCount; i++) {
        set(0, obj);
        assert.eq(get(0), obj);
        setNull(0);
        assert.eq(get(0), null);
    }

    assert.eq(grow(1), 1);
    set(1, obj);
    assert.eq(get(1), obj);

    assert.throws(() => set(2, obj), WebAssembly.RuntimeError, "Out of bounds table access (evaluating 'func(...args)')");
    assert.throws(() => get(2), WebAssembly.RuntimeError, "Out of bounds table access (evaluating 'func(...args)')");
}

await assert.asyncTest(test());
