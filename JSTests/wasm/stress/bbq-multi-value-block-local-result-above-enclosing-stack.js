import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const instance = await instantiate(`
(module
    (tag $e)
    (func $nop)
    (func $identity (param externref) (result externref)
        (local.get 0))

    (func (export "block") (param i32 i32) (result i32)
        (i32.const 100)
        (block (result i32 i32)
            (local.get 0)
            (i32.add (local.get 1) (i32.const 3)))
        (i32.sub)
        (i32.add))

    (func (export "loop") (param i32 i32) (result i32)
        (i32.const 100)
        (loop (result i32 i32)
            (local.get 0)
            (i32.add (local.get 1) (i32.const 3)))
        (i32.sub)
        (i32.add))

    (func (export "else") (param i32 i32) (result i32)
        (i32.const 100)
        (if (result i32 i32) (i32.eqz (local.get 0))
            (then
                (i32.const 1)
                (i32.const 2))
            (else
                (local.get 0)
                (i32.add (local.get 1) (i32.const 3))))
        (i32.sub)
        (i32.add))

    (func (export "catch") (param i32 i32) (result i32)
        (i32.const 100)
        (try (result i32 i32)
            (do
                (throw $e))
            (catch $e
                (local.get 0)
                (i32.add (local.get 1) (i32.const 3))))
        (i32.sub)
        (i32.add))

    (func (export "delegate") (param i32 i32) (result i32)
        (i32.const 100)
        (try (result i32 i32)
            (do
                (local.get 0)
                (i32.add (local.get 1) (i32.const 3)))
            (delegate 0))
        (i32.sub)
        (i32.add))

    (func (export "spilledTemp") (param i32 i32) (result i32)
        (i32.const 100)
        (block (result i32 i32)
            (local.get 0)
            (i32.add (local.get 1) (i32.const 3))
            (call $nop))
        (i32.sub)
        (i32.add))

    (func (export "pinnedLocal") (param i32 i32) (result i32)
        (local i32)
        (loop
            (local.set 0 (i32.add (local.get 0) (i32.const 1)))
            (local.set 2 (i32.add (local.get 2) (i32.const 1)))
            (br_if 0 (i32.lt_u (local.get 2) (i32.const 4))))
        (i32.const 100)
        (block (result i32 i32)
            (local.get 0)
            (i32.add (local.get 1) (i32.const 3)))
        (i32.sub)
        (i32.add))

    (func (export "twoEnclosingValues") (param i32 i32) (result i32)
        (i32.const 100)
        (i32.const 200)
        (block (result i32 i32 i32)
            (local.get 0)
            (local.get 1)
            (i32.add (local.get 1) (i32.const 3)))
        (i32.sub)
        (i32.sub)
        (i32.add)
        (i32.add))

    (func (export "f64") (param f64 f64) (result f64)
        (f64.const 100)
        (block (result f64 f64)
            (local.get 0)
            (f64.add (local.get 1) (f64.const 0.5)))
        (f64.sub)
        (f64.add))

    (func (export "externref") (param i64 externref) (result externref)
        (i32.const 100)
        (block (result i64 externref)
            (local.get 0)
            (call $identity (local.get 1)))
        (local.set 1)
        (drop)
        (drop)
        (local.get 1))
)
`, {}, { exceptions: true, reference_types: true });

/*
(module
    (func (export "tryTable") (param i32 i32) (result i32)
        (i32.const 100)
        (try_table (result i32 i32)
            (local.get 0)
            (i32.add (local.get 1) (i32.const 3)))
        (i32.sub)
        (i32.add))
)
*/
const tryTableInstance = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x0c, 0x02, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x00, 0x02, 0x7f, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x0c, 0x01, 0x08, 0x74, 0x72, 0x79, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00,
    0x0a, 0x14, 0x01, 0x12, 0x00, 0x41, 0xe4, 0x00, 0x1f, 0x01, 0x00, 0x20, 0x00, 0x20, 0x01, 0x41, 0x03, 0x6a, 0x0b, 0x6b, 0x6a, 0x0b,
])));

const object = { };

for (let i = 0; i < wasmTestLoopCount; ++i) {
    assert.eq(instance.exports.block(7, 5), 99);
    assert.eq(instance.exports.loop(7, 5), 99);
    assert.eq(instance.exports.else(7, 5), 99);
    assert.eq(instance.exports.catch(7, 5), 99);
    assert.eq(instance.exports.delegate(7, 5), 99);
    assert.eq(instance.exports.spilledTemp(7, 5), 99);
    assert.eq(instance.exports.pinnedLocal(7, 5), 103);
    assert.eq(instance.exports.twoEnclosingValues(7, 5), 310);
    assert.eq(instance.exports.f64(7, 5), 101.5);
    assert.eq(instance.exports.externref(6n, object), object);
    assert.eq(tryTableInstance.exports.tryTable(7, 5), 99);
}
