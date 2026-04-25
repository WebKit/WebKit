import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "sel_i32") (param i32 i32 i32) (result i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (select)
    )
    (func (export "sel_i64") (param i64 i64 i32) (result i64)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (select)
    )
    (func (export "sel_f32") (param f32 f32 i32) (result f32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (select)
    )
    (func (export "sel_f64") (param f64 f64 i32) (result f64)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (select)
    )
    (func (export "sel_ref") (param externref externref i32) (result externref)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (select (result externref))
    )
    (func (export "sel_i32_clhs") (param i32 i32) (result i32)
        (i32.const 42)
        (local.get 0)
        (local.get 1)
        (select)
    )
    (func (export "sel_i32_crhs") (param i32 i32) (result i32)
        (local.get 0)
        (i32.const 42)
        (local.get 1)
        (select)
    )
    (func (export "sel_i32_cboth") (param i32) (result i32)
        (i32.const 10)
        (i32.const 20)
        (local.get 0)
        (select)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { sel_i32, sel_i64, sel_f32, sel_f64, sel_ref, sel_i32_clhs, sel_i32_crhs, sel_i32_cboth } = instance.exports;
    const objA = { a: 1 };
    const objB = { b: 2 };

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(sel_i32(100, 200, 0), 200);
        assert.eq(sel_i32(100, 200, 1), 100);
        assert.eq(sel_i32(100, 200, -1), 100);

        assert.eq(sel_i64(100n, 200n, 0), 200n);
        assert.eq(sel_i64(100n, 200n, 1), 100n);

        assert.eq(sel_f32(1.5, 2.5, 0), 2.5);
        assert.eq(sel_f32(1.5, 2.5, 1), 1.5);

        assert.eq(sel_f64(1.5, 2.5, 0), 2.5);
        assert.eq(sel_f64(1.5, 2.5, 1), 1.5);

        assert.eq(sel_ref(objA, objB, 0), objB);
        assert.eq(sel_ref(objA, objB, 1), objA);
        assert.eq(sel_ref(null, objB, 1), null);

        assert.eq(sel_i32_clhs(7, 0), 7);
        assert.eq(sel_i32_clhs(7, 1), 42);

        assert.eq(sel_i32_crhs(7, 0), 42);
        assert.eq(sel_i32_crhs(7, 1), 7);

        assert.eq(sel_i32_cboth(0), 20);
        assert.eq(sel_i32_cboth(1), 10);
    }
}

await assert.asyncTest(test())
